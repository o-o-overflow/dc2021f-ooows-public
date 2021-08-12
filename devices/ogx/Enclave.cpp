#include "Enclave.h"

#include <sys/mman.h>

#include <cassert>
#include <csignal>

#include "ogx.h"
#include "spdlog/spdlog.h"

extern "C" const void* ogx_trampoline;
extern "C" const uint64_t ogx_trampoline_size;

Enclave::Enclave(const uint32_t id, const std::string& flag_path) : id_(id), label_(-1) {
    assert(id_ < NUM_ENCLAVES);
    assert((ENCLAVE_REGION_SIZE & 0xfff) == 0);

    auto log = spdlog::get("ogx");
    assert(log && "null logger");
    log->info("initializing enclave {}", id);

    log->debug("allocating memory regions for enclave {}", id_);
    code_ = mmap(nullptr, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_ == MAP_FAILED) {
        code_ = nullptr;
        throw std::runtime_error(fmt::format("unable to map enclave code region: {}", strerror(errno)));
    }
    data_ = mmap(nullptr, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data_ == MAP_FAILED) {
        data_ = nullptr;
        throw std::runtime_error(fmt::format("unable to map enclave data region: {}", strerror(errno)));
    }
    if (id_ == 0) {
        int fd = open(flag_path.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error(fmt::format("unable to open secret: {}", strerror(errno)));
        }
        flag_ = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, 0);
        if (flag_ == MAP_FAILED) {
            throw std::runtime_error(fmt::format("unable to map secret: {}", strerror(errno)));
        }
        close(fd);
    }
    exit_ = reinterpret_cast<uint8_t*>(data_) + 0x1024;
    stack_ = mmap(nullptr, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack_ == MAP_FAILED) {
        stack_ = nullptr;
        throw std::runtime_error(fmt::format("unable to map enclave stack region: {}", strerror(errno)));
    }
    trampoline_ = mmap(nullptr, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (trampoline_ == MAP_FAILED) {
        trampoline_ = nullptr;
        throw std::runtime_error(fmt::format("unable to map enclave trampoline region: {}", strerror(errno)));
    }

    // Set up the trampoline
    std::memcpy(trampoline_, &ogx_trampoline, ogx_trampoline_size);

    log->debug("allocating label for enclave {}", id_);
    label_ = pkey_alloc(0, 0);
    if (label_ < 0) {
        throw std::runtime_error(fmt::format("unable to allocate label: {}", strerror(errno)));
    }

    log->debug("labeling enclave {}", id_);
    if (pkey_mprotect(code_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, label_) != 0) {
        throw std::runtime_error(fmt::format("unable to label code region: {}", strerror(errno)));
    }
    if (pkey_mprotect(data_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, label_) != 0) {
        throw std::runtime_error(fmt::format("unable to label data region: {}", strerror(errno)));
    }
    if (pkey_mprotect(stack_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE, label_) != 0) {
        throw std::runtime_error(fmt::format("unable to label stack region: {}", strerror(errno)));
    }
    if (pkey_mprotect(trampoline_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_EXEC, label_) != 0) {
        throw std::runtime_error(fmt::format("unable to label trampoline region: {}", strerror(errno)));
    }
}

Enclave::~Enclave() {
    if (code_) {
        munmap(code_, ENCLAVE_REGION_SIZE);
        code_ = nullptr;
    }
    if (data_) {
        munmap(data_, ENCLAVE_REGION_SIZE);
        data_ = nullptr;
    }
    if (stack_) {
        munmap(stack_, ENCLAVE_REGION_SIZE);
        stack_ = nullptr;
    }
    if (trampoline_) {
        munmap(trampoline_, ENCLAVE_REGION_SIZE);
        trampoline_ = nullptr;
    }
}

/** Unprivileged execution mode flag. */
bool unprivileged = false;

std::vector<uint8_t> Enclave::Execute(const std::vector<uint8_t>& code, const std::vector<uint8_t>& data) {
    // Only execute one payload at a time
    std::lock_guard<std::mutex> lock(mutex_);

    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    log->info("enclave {}: loading payload", id_);

    log->debug("loading enclave {} code region with {} bytes", id_, code.size());
    if (code.size() > ENCLAVE_REGION_SIZE) {
        throw std::runtime_error(
            fmt::format("enclave code region size exceeded: {} > {}", code.size(), ENCLAVE_REGION_SIZE));
    }
    std::memset(code_, 0, ENCLAVE_REGION_SIZE);
    std::memcpy(code_, code.data(), code.size());

    log->debug("loading enclave {} data region with {} bytes", id_, data.size());
    if (data.size() > ENCLAVE_REGION_SIZE) {
        throw std::runtime_error(
            fmt::format("enclave data region size exceeded: {} > {}", data.size(), ENCLAVE_REGION_SIZE));
    }
    // std::memset(data_, 0, ENCLAVE_REGION_SIZE);
    if (flag_) {
        std::memcpy(data_, flag_, 4096);
    }
    std::memcpy(data_, data.data(), data.size());

    if (unprivileged) {
        // Check for WRPKRU instruction
        const auto wrpkru_offset = memmem(code_, ENCLAVE_REGION_SIZE, "\x0f\x01\xef", 3);
        if (wrpkru_offset) {
            log->critical(fmt::format("found illegal instruction at offset {}, aborting", wrpkru_offset));
            std::memset(code_, 0, ENCLAVE_REGION_SIZE);
            throw std::runtime_error("found illegal instruction, aborting load");
        }

        // Check for syscall instruction
        const auto syscall_offset = memmem(code_, ENCLAVE_REGION_SIZE, "\x0f\x05", 2);
        if (syscall_offset) {
            log->critical(fmt::format("found illegal instruction at offset {}, aborting", syscall_offset));
            std::memset(code_, 0, ENCLAVE_REGION_SIZE);
            throw std::runtime_error("found illegal instruction, aborting load");
        }

        // Check for sysenter instruction
        const auto sysenter_offset = memmem(code_, ENCLAVE_REGION_SIZE, "\x0f\x34", 2);
        if (sysenter_offset) {
            log->critical(fmt::format("found illegal instruction at offset {}, aborting", sysenter_offset));
            std::memset(code_, 0, ENCLAVE_REGION_SIZE);
            throw std::runtime_error("found illegal instruction, aborting load");
        }

        // Check for int 0x80 instruction
        const auto int80_offset = memmem(code_, ENCLAVE_REGION_SIZE, "\xcd\x80", 2);
        if (int80_offset) {
            log->critical(fmt::format("found illegal instruction at offset {}, aborting", int80_offset));
            std::memset(code_, 0, ENCLAVE_REGION_SIZE);
            throw std::runtime_error("found illegal instruction, aborting load");
        }
    }

    unprivileged = true;

    // Mark the enclave executable
    if (mprotect(code_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_EXEC) != 0) {
        throw std::runtime_error(fmt::format("unable to set enclave executable: {}", strerror(errno)));
    }

    volatile int exec_result = 0;
    std::vector<uint8_t> result;
    *exit_ = 0;

    try {
        // We need to manage privileges when invoking the enclave and do some housekeeping like setting up a stack and
        // passing a pointer to the data segment.  We use a trampoline for this.  An exec_result can be returned by
        // setting the return value to a valid length and placing the return data in the data segment.
        log->debug(
            "enclave {}: invoking code={} data={} stack={} trampoline={}", id_, code_, data_, stack_, trampoline_);
        asm volatile(
            "movl %2, %%edi\n"
            "movq %3, %%rsi\n"
            "movq %4, %%rdx\n"
            "movq %5, %%rcx\n"
            "callq *%1\n"
            ".ascii \"OOOGXBBY\"\n"
            : "=a"(exec_result)
            : "r"(trampoline_), "r"(label_), "r"(code_), "r"(data_), "r"(stack_)
            : "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "r13", "rbp", "memory");
        log->debug("enclave {}: returning from {} with return {}", id_, code_, exec_result);

        // Mark the enclave non-executable
        if (mprotect(code_, ENCLAVE_REGION_SIZE, PROT_READ | PROT_WRITE) != 0) {
            throw std::runtime_error(fmt::format("unable to set enclave non-executable: {}", strerror(errno)));
        }

        // Extract result
        if (exec_result > 0 && exec_result < 128) {
            log->debug("enclave {}: storing exec_result", id_);
            result.reserve(exec_result);
            std::copy(
                reinterpret_cast<uint8_t*>(data_),
                reinterpret_cast<uint8_t*>(data_) + exec_result,
                std::back_inserter(result));
        } else if (exec_result < 0) {
            throw std::runtime_error(fmt::format("enclave returned error code {}", exec_result));
        }

        // Clean up
        log->debug("enclave {}: resetting memory", id_);
        std::memset(code_, 0, ENCLAVE_REGION_SIZE);
        std::memset(data_, 0, ENCLAVE_REGION_SIZE);
        std::memset(stack_, 0, ENCLAVE_REGION_SIZE);
    } catch (std::exception& e) {
        // BUG: If the enclave returns a value < 1 memory will be leaked to the next enclave.
        log->warn("enclave {}: {}", id_, e.what());
        return {};
    }

    return result;
}

void Enclave::Stop() const {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");
    log->debug("enclave {}: setting exit flag at {}", id_, reinterpret_cast<void*>(exit_));
    *exit_ = 1;
}
