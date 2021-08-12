#include "Device.h"

#include <utility>

#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using namespace std::chrono_literals;
using boost::asio::local::stream_protocol;

Device::Device(
    void*& fault_addr,
    uint32_t& fault_key,
    std::vector<uint8_t> pub_key,
    std::vector<uint8_t> sec_key,
    std::string flag_path,
    size_t pool_size)
    : fault_addr_(fault_addr),
      fault_key_(fault_key),
      pub_key_(std::move(pub_key)),
      sec_key_(std::move(sec_key)),
      flag_path_(std::move(flag_path)),
      enclave_pool_(std::make_unique<ThreadPool>()),
      process_events_(false) {
    assert(pub_key_.size() == crypto_box_PUBLICKEYBYTES && "ill-formed public key");
    assert(sec_key_.size() == crypto_box_SECRETKEYBYTES && "ill-formed secret key");

    for (auto i = 0; i < NUM_ENCLAVES; i++) {
        enclaves_.emplace_back(std::make_unique<Enclave>(i, flag_path_));
    }
    enclave_pool_->Init(pool_size);
}

Device::~Device() {
    enclave_pool_->Shutdown();
}

void Device::ProcessEvents(const std::optional<size_t> iterations) {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    auto i = 0U;
    process_events_ = true;
    while (process_events_ && (!iterations.has_value() || i < *iterations)) {
        if ((i % 100) == 0) {
            log->debug("device event loop iteration {}", i);
        }
        ++i;

        // Check for a fault
        if (fault_addr_) {
            if (fault_key_) {
                // Attribute by protection key
                log->critical("fault detected, aborting enclave with protection key {}", fault_key_);
                for (auto& e : enclaves_) {
                    if (e->label_ == fault_key_) {
                        e->Stop();
                        break;
                    }
                }
            } else {
                // Attribute by fault address
                // BUG: Actually, we treat the fault address as the key also
                log->critical("fault detected, aborting enclave with fault address {}", fault_addr_);
                for (auto& e : enclaves_) {
                    // log->debug("checking enclave id={} label={}", e->id_, e->label_);
                    if (reinterpret_cast<void*>(e->label_) == fault_addr_) {
                        e->Stop();
                        break;
                    }
                }
            }

            // Reset fault vars
            fault_addr_ = nullptr;
            fault_key_ = 0;
        }

        try {
            // Try to read a device event
            if (CanReadEvent()) {
                // Read and handle the event
                auto [sender_pub_key, event] = ReadEvent();
                if (auto load_event = std::dynamic_pointer_cast<LoadEvent>(event)) {
                    // Load event
                    ProcessLoadEvent(sender_pub_key, load_event);
                }
            } else if (enclave_futures_.empty()) {
                std::this_thread::sleep_for(100ms);
            }
        } catch (std::exception& e) {
            log->critical("unable to read device event");
            log->critical("{}", e.what());
        }

        try {
            // Check enclave futures
            for (auto& x : enclave_futures_) {
                auto& [enclave_id, sender_pub_key, f] = x;
                if (f.wait_for(100ms) == std::future_status::ready) {
                    // Get the response and write it to the driver
                    auto response = f.get();
                    ResponseEvent event(enclave_id, response);
                    WriteEvent(sender_pub_key, &event);
                }
            }

            // Prune old futures
            enclave_futures_.erase(
                std::remove_if(
                    enclave_futures_.begin(),
                    enclave_futures_.end(),
                    [](std::tuple<size_t, std::vector<uint8_t>, ThreadPool::Future>& x) {
                        auto& [enclave_id, sender_pub_key, f] = x;
                        return !f.valid();
                    }),
                enclave_futures_.end());
        } catch (std::exception& e) {
            log->critical("unable to write enclave result");
            log->critical("{}", e.what());
        }
    }
}

void Device::ProcessLoadEvent(std::vector<uint8_t> sender, std::shared_ptr<LoadEvent>& e) {
    if (e->id_ >= enclaves_.size()) {
        throw std::runtime_error(fmt::format("invalid enclave ID in load event: {} >= {}", e->id_, enclaves_.size()));
    }

    auto& enclave = enclaves_[e->id_];
    auto fn = [e, &enclave]() {
        // Copy event data into the closure
        auto code = e->code_;
        auto data = e->data_;
        return enclave->Execute(code, data);
    };
    auto f = enclave_pool_->Schedule(fn);
    enclave_futures_.emplace_back(e->id_, std::move(sender), std::move(f));
}

void Device::Stop() {
    process_events_ = false;
}

TestDevice::TestDevice(
    void*& fault_addr,
    uint32_t& fault_key,
    std::vector<uint8_t> pub_key,
    std::vector<uint8_t> sec_key,
    std::string flag_path,
    boost::asio::io_context& io_context,
    const std::string& socket_path)
    : Device(fault_addr, fault_key, std::move(pub_key), std::move(sec_key), std::move(flag_path), 2),
      acceptor_(io_context, stream_protocol::endpoint(socket_path)) {}

bool TestDevice::CanReadEvent() {
    auto log = spdlog::get("ogx");
    assert(log && "null log");

    // Check if we have a current session; if we don't, this is the only time we block
    if (!socket_) {
        log->debug("blocked on new session while checking for read availability");
        socket_ = acceptor_.accept();
    }

    return socket_->available() > 0;
}

std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>> TestDevice::ReadEvent() {
    auto log = spdlog::get("ogx");
    assert(log && "null log");

    while (true) {
        // Check if we have a current session.
        if (!socket_) {
            socket_ = acceptor_.accept();
        }

        try {
            // Read an event length
            std::array<uint8_t, 2> length{};
            boost::asio::read(*socket_, boost::asio::buffer(length));

            // Read an event
            std::vector<uint8_t> data(*reinterpret_cast<uint16_t*>(length.data()));
            boost::asio::read(*socket_, boost::asio::buffer(data));

            // Parse and return
            return Event::Decrypt(sec_key_, data);
        } catch (std::exception& e) {
            log->warn("unable to read event, resetting socket and retrying");
            log->warn(fmt::format("cause: {}", e.what()));
            socket_->close();
            socket_ = std::nullopt;
        }
    }
}

void TestDevice::WriteEvent(const std::vector<uint8_t>& receiver_pub_key, Event* event) {
    assert(event && "null event");

    auto log = spdlog::get("ogx");
    assert(log && "null log");

    // Check if we have a current session
    if (!socket_) {
        // This shouldn't happen?  We just drop the event.
        return;
    }

    try {
        const auto data = event->Serialize();
        const auto encrypted_data = Event::Encrypt(pub_key_, sec_key_, receiver_pub_key, data);
        std::array<uint8_t, 2> length{};
        *reinterpret_cast<uint16_t*>(length.data()) = encrypted_data.size();
        boost::asio::write(*socket_, boost::asio::buffer(length));
        boost::asio::write(*socket_, boost::asio::buffer(encrypted_data));
    } catch (std::exception& e) {
        log->warn("unable to write event, resetting socket");
        log->warn(fmt::format("cause: {}", e.what()));
        socket_->close();
        socket_ = std::nullopt;
    }
}
