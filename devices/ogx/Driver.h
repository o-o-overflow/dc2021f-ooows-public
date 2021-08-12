#pragma once

#include <boost/asio.hpp>
#include <optional>

#include "Event.h"

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!! DRIVERS SHOULD NOT BE DISTRIBUTED IN THE GAME BINARY !!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

class Driver {
public:
    virtual ~Driver() = default;
    virtual std::optional<std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>>> ReadEvent() = 0;
    virtual void WriteEvent(Event* event) = 0;
};

class TestDriver : public Driver {
public:
    explicit TestDriver(
        std::vector<uint8_t> ogx_pub_key,
        std::vector<uint8_t> pub_key,
        std::vector<uint8_t> sec_key,
        boost::asio::io_context& io_context,
        const char* socket_path);
    ~TestDriver() override = default;
    std::optional<std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>>> ReadEvent() override;
    void WriteEvent(Event* event) override;

private:
    const std::vector<uint8_t> ogx_pub_key_;
    const std::vector<uint8_t> pub_key_;
    const std::vector<uint8_t> sec_key_;
    boost::asio::local::stream_protocol::socket socket_;
};
