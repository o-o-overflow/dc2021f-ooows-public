#include "Event.h"

#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using nlohmann::json;

std::shared_ptr<Event> Event::Deserialize(const std::vector<uint8_t>& data) {
    try {
        auto object = json::from_cbor(data);
        if (object["t"].get<std::string>() == "load") {
            return std::make_shared<LoadEvent>(
                object["i"].get<uint32_t>(),
                object["c"].get<std::vector<uint8_t>>(),
                object["d"].get<std::vector<uint8_t>>());
        } else if (object.contains("t") && object["t"].get<std::string>() == "response") {
            return std::make_shared<ResponseEvent>(
                object["i"].get<uint32_t>(), object["r"].get<std::vector<uint8_t>>());
        }
    } catch (std::exception& e) {
        throw std::runtime_error(fmt::format("unable to parse event during deserialization: {}", e.what()));
    }

    throw std::runtime_error("unknown event type during deserialization");
}

std::vector<uint8_t> Event::Encrypt(
    const std::vector<uint8_t>& sender_pub_key,
    const std::vector<uint8_t>& sender_sec_key,
    const std::vector<uint8_t>& receiver_pub_key,
    const std::vector<uint8_t>& message) {
    if (sender_sec_key.size() != crypto_box_SECRETKEYBYTES) {
        throw std::runtime_error("sender secret key is ill-formed");
    }
    if (receiver_pub_key.size() != crypto_box_PUBLICKEYBYTES) {
        throw std::runtime_error("receiver public key is ill-formed");
    }

    std::vector<uint8_t> nonce(crypto_box_NONCEBYTES);
    randombytes_buf(nonce.data(), nonce.size());
    std::vector<uint8_t> ciphertext(crypto_box_MACBYTES + message.size());
    if (crypto_box_easy(
            ciphertext.data(),
            message.data(),
            message.size(),
            nonce.data(),
            receiver_pub_key.data(),
            sender_sec_key.data())) {
        throw std::runtime_error("unable to encrypt message");
    }

    json object;
    object["s"] = sender_pub_key;
    object["m"] = ciphertext;
    object["n"] = nonce;
    return json::to_cbor(object);
}

std::tuple<std::vector<uint8_t>, std::shared_ptr<Event>> Event::Decrypt(
    const std::vector<uint8_t>& receiver_sec_key, const std::vector<uint8_t>& data) {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    if (receiver_sec_key.size() != crypto_box_SECRETKEYBYTES) {
        throw std::runtime_error("receiver secret key is ill-formed");
    }

    auto object = json::from_cbor(data);
    // log->debug("getting sender key");
    auto sender_pub_key = object["s"].get<std::vector<uint8_t>>();
    // log->debug("getting ciphertext");
    auto ciphertext = object["m"].get<std::vector<uint8_t>>();
    // log->debug("getting nonce");
    auto nonce = object["n"].get<std::vector<uint8_t>>();
    if (sender_pub_key.size() != crypto_box_PUBLICKEYBYTES) {
        throw std::runtime_error("sender_pub_key public key is ill-formed");
    }
    if (ciphertext.size() < crypto_box_MACBYTES) {
        throw std::runtime_error("ciphertext is truncated");
    }
    if (nonce.size() != crypto_box_NONCEBYTES) {
        throw std::runtime_error("ill-formed nonce");
    }

    std::vector<uint8_t> plaintext(ciphertext.size() - crypto_box_MACBYTES);
    if (crypto_box_open_easy(
            plaintext.data(),
            ciphertext.data(),
            ciphertext.size(),
            nonce.data(),
            sender_pub_key.data(),
            receiver_sec_key.data())) {
        throw std::runtime_error("unable to decrypt message");
    }

    return std::make_tuple(sender_pub_key, Deserialize(plaintext));
}

LoadEvent::LoadEvent(const uint32_t id, std::vector<uint8_t> code, std::vector<uint8_t> data)
    : id_(id), code_(std::move(code)), data_(std::move(data)) {
    if (id_ >= NUM_ENCLAVES) {
        throw std::runtime_error(fmt::format("requested enclave ID too large: {} > {}", id_, NUM_ENCLAVES - 1));
    }
    if (code_.size() > ENCLAVE_REGION_SIZE) {
        throw std::runtime_error(
            fmt::format("enclave code region size exceeded: {} > {}", code_.size(), ENCLAVE_REGION_SIZE));
    }
    if (data_.size() > ENCLAVE_REGION_SIZE) {
        throw std::runtime_error(
            fmt::format("enclave data region size exceeded: {} > {}", data.size(), ENCLAVE_REGION_SIZE));
    }
}

std::vector<uint8_t> LoadEvent::Serialize() {
    json object;
    object["t"] = "load";
    object["i"] = id_;
    object["c"] = code_;
    object["d"] = data_;
    return json::to_cbor(object);
}

ResponseEvent::ResponseEvent(const uint32_t id, std::vector<uint8_t> response)
    : id_(id), response_(std::move(response)) {}

std::vector<uint8_t> ResponseEvent::Serialize() {
    json object;
    object["t"] = "response";
    object["i"] = id_;
    object["r"] = response_;
    return json::to_cbor(object);
}
