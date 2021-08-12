#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

/**
 * Enclave.
 */
class Enclave {
public:
    /**
     * Initialize a new enclave.
     *
     * @param id Identifier.
     * @param flag_path Flag path.
     */
    explicit Enclave(uint32_t id, const std::string& flag_path);

    /**
     * Destructor.
     */
    ~Enclave();

    /**
     * Execute the enclave.
     *
     * @param code Code.
     * @param data Data.
     * @return Result.
     */
    std::vector<uint8_t> Execute(const std::vector<uint8_t>& code, const std::vector<uint8_t>& data);

    /**
     * Stop the enclave.
     *
     * It is up to the payload to check the exit flag.
     */
    void Stop() const;

    /** Enclave identifier. */
    const uint32_t id_;
    /** Enclave protection key label. */
    int32_t label_;
    /** Code region. */
    void* code_;
    /** Data region. */
    void* data_;
    /** Stack region. */
    void* stack_;
    /** Trampoline. */
    void* trampoline_;
    /** Mutex. */
    std::mutex mutex_;
    /** Exit flag. */
    uint8_t* exit_;
    /** Flag region. */
    void* flag_{};
};
