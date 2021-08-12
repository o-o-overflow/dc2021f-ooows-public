#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <optional>

#include "Enclave.h"
#include "Event.h"
#include "ThreadPool.h"
#include "virtio.hpp"

/**
 * Device interface.
 */
class Device {
public:
    /**
     * Create a new device.
     *
     * @param fault_addr Fault address set by signal handler.
     * @param fault_key Fault key set by signal handler.
     * @param pub_key Public key.
     * @param sec_key Secret key.
     * @param pool_size Enclave thread pool size.
     */
    explicit Device(
        void*& fault_addr,
        uint32_t& fault_key,
        std::vector<uint8_t> pub_key,
        std::vector<uint8_t> sec_key,
        std::string flag_path,
        size_t pool_size);

    /**
     * Destructor.
     */
    virtual ~Device();

    /**
     * Process events.
     *
     * @param iterations Event processing iterations.
     */
    void ProcessEvents(std::optional<size_t> iterations = std::nullopt);

    /**
     * Process a load event.
     *
     * @param sender Sender key.
     * @param e Event.
     */
    void ProcessLoadEvent(std::vector<uint8_t> sender, std::shared_ptr<LoadEvent>& e);

    /**
     * Stop the device.
     */
    void Stop();

    /**
     * Check whether an event can be read from the driver.
     *
     * @return True if an event is available.
     */
    virtual bool CanReadEvent() = 0;

    /**
     * Read an event from the driver.
     *
     * This should block if no event is available.
     *
     * @return Event.
     */
    virtual std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>> ReadEvent() = 0;

    /**
     * Write an event to the driver.
     *
     * This should block until the event is successfully written.
     *
     * @param receiver_pub_key Receiver key.
     * @param event Event.
     */
    virtual void WriteEvent(const std::vector<uint8_t>& receiver_pub_key, Event* event) = 0;

protected:
    /** Fault address. */
    void*& fault_addr_;
    /** Fault key. */
    uint32_t& fault_key_;
    /** Public key. */
    const std::vector<uint8_t> pub_key_;
    /** Secret key. */
    const std::vector<uint8_t> sec_key_;
    /** Flag path. */
    const std::string flag_path_;
    /** Enclaves. */
    std::vector<std::unique_ptr<Enclave>> enclaves_;
    /** Enclave thread pool. */
    std::shared_ptr<ThreadPool> enclave_pool_;
    /** Enclave execution futures. */
    std::vector<std::tuple<size_t, std::vector<uint8_t>, ThreadPool::Future>> enclave_futures_;
    /** Process events flag. */
    std::atomic_bool process_events_;
};

/**
 * Test device interface that does not require a guest.
 */
class TestDevice : public Device {
public:
    /**
     * Create a new test device.
     *
     * @param fault_addr Fault address set by signal handler.
     * @param fault_key Fault key set by signal handler.
     * @param pub_key Public key.
     * @param sec_key Secret key.
     * @param flag_path Flag path.
     * @param io_context IO context.
     * @param socket_path Device socket.
     */
    explicit TestDevice(
        void*& fault_addr,
        uint32_t& fault_key,
        std::vector<uint8_t> pub_key,
        std::vector<uint8_t> sec_key,
        std::string flag_path,
        boost::asio::io_context& io_context,
        const std::string& socket_path);

    /**
     * Destructor.
     */
    ~TestDevice() override = default;

    bool CanReadEvent() override;

    std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>> ReadEvent() override;

    void WriteEvent(const std::vector<uint8_t>& receiver_pub_key, Event* event) override;

private:
    /** UDS acceptor. */
    boost::asio::local::stream_protocol::acceptor acceptor_;
    /** Current session. */
    std::optional<boost::asio::local::stream_protocol::socket> socket_;
};
