#pragma once

#include <sodium.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "nlohmann/json.hpp"
#include "ogx.h"

/**
 * Event.
 */
class Event {
public:
    /**
     * Destructor.
     */
    virtual ~Event() = default;

    /**
     * Serialize an event.
     *
     * @return CBOR.
     */
    virtual std::vector<uint8_t> Serialize() = 0;

    /**
     * Deserialize an event.
     *
     * @param data Event data.
     * @return Event.
     */
    static std::shared_ptr<Event> Deserialize(const std::vector<uint8_t>& data);

    /**
     * Encrypt an event.
     *
     * Events are encrypted using libsodium's box primitive.
     *
     * @param sender_sec_key Sender key.
     * @param receiver_pub_key Receiver key.
     * @param message Message, expected to be a CBOR blob output by Event::Serialize.
     */
    static std::vector<uint8_t> Encrypt(
        const std::vector<uint8_t>& sender_pub_key,
        const std::vector<uint8_t>& sender_sec_key,
        const std::vector<uint8_t>& receiver_pub_key,
        const std::vector<uint8_t>& message);

    /**
     * Decrypt an event.
     *
     * This is the inverse of encryption plus deserialization.
     *
     * @param receiver_sec_key Receiver key.
     * @param data Ciphertext.
     */
    static std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>> Decrypt(
        const std::vector<uint8_t>& receiver_sec_key, const std::vector<uint8_t>& data);
};

/**
 * Load event.
 */
class LoadEvent : public Event {
public:
    /**
     * Create a load event.
     *
     * @param id Enclave ID.
     * @param code Enclave code.
     * @param data Enclave data.
     */
    explicit LoadEvent(uint32_t id, std::vector<uint8_t> code, std::vector<uint8_t> data);

    /**
     * Destructor.
     */
    ~LoadEvent() override = default;

    std::vector<uint8_t> Serialize() override;

    /** Enclave ID. */
    const uint32_t id_;
    /** Code. */
    const std::vector<uint8_t> code_;
    /** Data. */
    const std::vector<uint8_t> data_;
};

/**
 * Response event.
 */
class ResponseEvent : public Event {
public:
    /**
     * Create a response event.
     *
     * @param id Enclave ID.
     * @param response Enclave response.
     */
    explicit ResponseEvent(uint32_t id, std::vector<uint8_t> response);

    /**
     * Destructor.
     */
    ~ResponseEvent() override = default;

    std::vector<uint8_t> Serialize() override;

    /** Enclave ID. */
    const uint32_t id_;
    /** Response. */
    const std::vector<uint8_t> response_;
};
