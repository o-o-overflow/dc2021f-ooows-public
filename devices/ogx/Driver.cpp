#include "Driver.h"

#include <fstream>
#include <optional>

#include "spdlog/spdlog.h"

TestDriver::TestDriver(
    std::vector<uint8_t> ogx_pub_key,
    std::vector<uint8_t> pub_key,
    std::vector<uint8_t> sec_key,
    boost::asio::io_context& io_context,
    const char* socket_path)
    : ogx_pub_key_(std::move(ogx_pub_key)),
      pub_key_(std::move(pub_key)),
      sec_key_(std::move(sec_key)),
      socket_(io_context) {
    using boost::asio::local::stream_protocol;
    socket_.connect(stream_protocol::endpoint(socket_path));
}

static void WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    auto log = spdlog::get("ogx");
    assert(log && "null log");
    log->debug("writing {}", path);
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::optional<std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>>> TestDriver::ReadEvent() {
    auto log = spdlog::get("ogx");
    assert(log && "null log");

    static size_t read_event_id;

    try {
        // Read an event length
        std::array<uint8_t, 2> length{};
        boost::asio::read(socket_, boost::asio::buffer(length));

        // Read an event
        std::vector<uint8_t> data(*reinterpret_cast<uint16_t*>(length.data()));
        boost::asio::read(socket_, boost::asio::buffer(data));

        WriteFile(fmt::format("ogx_e_r_{}", read_event_id), data);
        ++read_event_id;

        // Parse and return
        try {
            return std::make_optional(Event::Decrypt(sec_key_, data));
        } catch (std::exception& e) {
            log->warn(fmt::format("{}", e.what()));
            return std::nullopt;
        }
    } catch (std::exception& e) {
        log->warn("unable to read event, resetting socket and retrying");
        log->warn(fmt::format("cause: {}", e.what()));
        socket_.close();
    }

    return std::nullopt;
}

void TestDriver::WriteEvent(Event* event) {
    assert(event && "null event");

    auto log = spdlog::get("ogx");
    assert(log && "null log");

    static size_t write_event_id;

    try {
        const auto data = event->Serialize();
        const auto encrypted_data = Event::Encrypt(pub_key_, sec_key_, ogx_pub_key_, data);
        WriteFile(fmt::format("ogx_e_w_{}", write_event_id), encrypted_data);
        ++write_event_id;
        std::array<uint8_t, 2> length{};
        *reinterpret_cast<uint16_t*>(length.data()) = encrypted_data.size();
        boost::asio::write(socket_, boost::asio::buffer(length));
        boost::asio::write(socket_, boost::asio::buffer(encrypted_data));
    } catch (std::exception& e) {
        log->warn("unable to write event, resetting socket");
        log->warn(fmt::format("cause: {}", e.what()));
        socket_.close();
    }
}
