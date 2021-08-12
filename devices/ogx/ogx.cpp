#include "ogx.h"

#include <cassert>
#include <csignal>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>

#include "Enclave.h"
#include "Event.h"
#include "ThreadPool.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"
#include "virtio.hpp"

static const char* const OGX_PUB_KEY_PATH = "devices-bin/ogx.pub";
static const char* const OGX_SEC_KEY_PATH = "devices-bin/ogx.sec";
static const char* const OGX_ENCLAVE_FLAG_PATH = "devices-bin/ogx_enclave_flag.bin";
static const char* const OGX_FLAG_PATH = "/flag";

using namespace std::chrono_literals;

/**
 * Guest device interface.
 */
class GuestDevice : public MMIOVirtioDev {
public:
    /**
     * Create a new guest device.
     *
     * @param fault_addr Fault address set by signal handler.
     * @param fault_key Fault key set by signal handler.
     * @param pub_key Public key.
     * @param sec_key Secret key.
     * @param flag_path Flag path.
     */
    explicit GuestDevice(
        void*& fault_addr,
        uint32_t& fault_key,
        std::vector<uint8_t> pub_key,
        std::vector<uint8_t> sec_key,
        std::string flag_path);

    ~GuestDevice() override = default;

    int got_data(uint16_t vq_idx) override;
    int config_space_read(uint64_t offset, uint64_t* out, uint32_t size) override;
    int config_space_write(uint64_t offset, uint64_t data, uint32_t size) override;

    /**
     * Execute the device.
     */
    void Execute();

    /**
     * Read a message from the driver.
     *
     * @param buffer Message buffer.
     * @return Zero if successful.
     */
    int ReadDriverMessage(VirtBuf* buffer);

    /**
     * Handle a load event.
     *
     * @param sender Sender key.
     * @param event Event.
     */
    void OnLoadEvent(const std::vector<uint8_t>& sender, LoadEvent* event);

    static const uint32_t MMIO_START = 0xefff0000;
    static const size_t VQ_READ = 0;
    static const size_t VQ_WRITE = 1;
    static const size_t NUM_VQS = 2;
    static const size_t READY_IRQ = 2;

private:
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
    /** Response buffers. */
    std::deque<VirtBuf*> response_buffers_;
    /** Mutex. */
    std::mutex device_mutex_;
};

GuestDevice::GuestDevice(
    void*& fault_addr,
    uint32_t& fault_key,
    std::vector<uint8_t> pub_key,
    std::vector<uint8_t> sec_key,
    std::string flag_path)
    : MMIOVirtioDev(MMIO_START, NUM_VQS),
      fault_addr_(fault_addr),
      fault_key_(fault_key),
      pub_key_(std::move(pub_key)),
      sec_key_(std::move(sec_key)),
      flag_path_(std::move(flag_path)),
      enclave_pool_(std::make_unique<ThreadPool>()) {
    assert(pub_key_.size() == crypto_box_PUBLICKEYBYTES && "ill-formed public key");
    assert(sec_key_.size() == crypto_box_SECRETKEYBYTES && "ill-formed secret key");

    m_device_id = VIRTIO_OGX_DEV;

    // We don't have features
    set_device_features(0);

    // We don't have a configuration region
    set_config_space(nullptr, 0);

    for (auto i = 0; i < NUM_ENCLAVES; i++) {
        enclaves_.emplace_back(std::make_unique<Enclave>(i, flag_path_));
    }
    enclave_pool_->Init(4);
}

int GuestDevice::got_data(uint16_t vq_idx) {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    int ret = 0;

    switch (vq_idx) {
        case VQ_READ: {
            log->debug("read buffer notification");
            std::lock_guard<std::mutex> lock(device_mutex_);
            auto buffer = get_buf(vq_idx);
            while (buffer) {
                log->debug("pushing a read buffer");
                response_buffers_.push_back(buffer);
                buffer = get_buf(vq_idx);
            }
            break;
        }
        case VQ_WRITE: {
            log->debug("write buffer notification");
            auto buffer = get_buf(vq_idx);
            while (buffer) {
                ReadDriverMessage(buffer);
                ret = put_buf(vq_idx, buffer);
                if (!ret) {
                    break;
                }
                buffer = get_buf(vq_idx);
            }
            break;
        }
        default:
            log->debug("unknown data notification on queue index {}", vq_idx);
            break;
    }

    return ret;
}

int GuestDevice::config_space_read(uint64_t offset, uint64_t* out, uint32_t size) {
    // Unsupported
    return -1;
}

int GuestDevice::config_space_write(uint64_t offset, uint64_t data, uint32_t size) {
    // Unsupported
    return -1;
}

void GuestDevice::Execute() {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    while (true) {
        // Check for a fault
        if (fault_addr_) {
            std::lock_guard<std::mutex> lock(device_mutex_);

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
            std::lock_guard<std::mutex> lock(device_mutex_);

            // Check enclave futures
            for (auto& x : enclave_futures_) {
                if (response_buffers_.empty()) {
                    break;
                }

                auto& [enclave_id, sender_pub_key, f] = x;
                if (f.valid() && f.wait_for(100ms) == std::future_status::ready) {
                    // Get the response and write it to the driver
                    log->debug("writing enclave {} response to driver", enclave_id);
                    auto response = f.get();
                    ResponseEvent event(enclave_id, response);
                    const auto event_data = event.Serialize();
                    const auto ciphertext = Event::Encrypt(pub_key_, sec_key_, sender_pub_key, event_data);
                    const uint16_t ciphertext_size = ciphertext.size();
                    auto buffer = response_buffers_.front();
                    response_buffers_.pop_front();
                    // log->debug("writing size ({} bytes)", ciphertext_size);
                    buffer->write(0, (void*)&ciphertext_size, sizeof(ciphertext_size));
                    // log->debug("writing ciphertext");
                    buffer->write(sizeof(ciphertext_size), (void*)ciphertext.data(), ciphertext.size());
                    auto sum = 0;
                    for (auto i = 0; i < ciphertext_size; ++i) {
                        sum ^= ciphertext[i];
                    }
                    // log->debug("ciphertext sum: {}", sum);
                    // log->debug("putting buffer");
                    put_buf(VQ_READ, buffer);
                    // log->debug("raising interrupt");
                    send_irq(READY_IRQ);
                    // log->debug("driver notified");
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

        // Try to not starve other threads
        std::this_thread::sleep_for(100ms);
    }
}

int GuestDevice::ReadDriverMessage(VirtBuf* buffer) {
    assert(buffer && "null buffer");
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    try {
        log->debug("reading a driver message");
        uint16_t message_size;
        buffer->readU(&message_size, sizeof(message_size));
        log->debug("reading {} byte message", message_size);
        std::vector<uint8_t> message(message_size);
        buffer->readU(message.data(), message.size());
        log->debug("decrypting message", message_size);
        auto [sender, event] = Event::Decrypt(sec_key_, message);
        if (auto load_event = std::dynamic_pointer_cast<LoadEvent>(event)) {
            OnLoadEvent(sender, load_event.get());
        }
        return 0;
    } catch (std::exception& e) {
        log->warn(fmt::format("unable to process driver message: {}", e.what()));
        return 1;
    }
}

void GuestDevice::OnLoadEvent(const std::vector<uint8_t>& sender, LoadEvent* event) {
    assert(event && "null event");
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    log->debug("handling a load event for enclave {}", event->id_);

    std::lock_guard<std::mutex> lock(device_mutex_);

    if (event->id_ >= enclaves_.size()) {
        throw std::runtime_error(
            fmt::format("invalid enclave ID in load event: {} >= {}", event->id_, enclaves_.size()));
    }

    auto& enclave = enclaves_[event->id_];
    auto code = event->code_;
    auto data = event->data_;
    auto fn = [code, data, &enclave]() {
        return enclave->Execute(code, data);
    };
    log->debug("scheduling payload on enclave {}", event->id_);
    auto f = enclave_pool_->Schedule(fn);
    enclave_futures_.emplace_back(event->id_, sender, std::move(f));
}

/**
 * Read a file into a vector.
 *
 * @param path Path.
 * @return Vector.
 */
static std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<uint8_t> data(std::istreambuf_iterator<char>(input), {});
    return data;
}

/**
 * Sandbox the process.
 */
// static void Sandbox() {
//     auto log = spdlog::get("ogx");
//     assert(log && "null logger");
//     log->info("sandboxing device");
//
//     auto ctx = seccomp_init(SCMP_ACT_KILL);
//     if (!ctx) {
//         throw std::runtime_error("unable to initialize seccomp filters");
//     }
//
// #define ADD(x)                                                                    \
//     {                                                                             \
//         if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(x), 0) < 0) {          \
//             throw std::runtime_error("unable to add syscall to allow list: " #x); \
//         }                                                                         \
//     }
//
//     ADD(clock_nanosleep);
//     ADD(epoll_ctl);
//     ADD(getrandom);
//     ADD(futex);
//     ADD(ioctl);
//     ADD(mmap);
//     ADD(mprotect);
//     ADD(munmap);
//     ADD(recvfrom);
//     ADD(rt_sigreturn);
//     ADD(sendto);
//     ADD(write);
//
// #undef ADD
//
//     if (seccomp_load(ctx) < 0) {
//         throw std::runtime_error("unable to load seccomp filters");
//     }
//
//     seccomp_release(ctx);
// }

/** Fault address. */
static void* fault_addr;

/** Fault key if applicable. */
static uint32_t fault_key;

/**
 * Signal handler.
 *
 * @param n Signal.
 * @param info Signal info.
 * @param context User context.
 */
static void signal_handler(int n, siginfo_t* info, void* context) {
    asm volatile(
        "movq $0, %%rax\n"
        "movq $0, %%rcx\n"
        "movq $0, %%rdx\n"
        "wrpkru\n"
        :
        :
        : "rax", "rcx", "rdx");

    auto* u = static_cast<ucontext_t*>(context);

    switch (n) {
        case SIGILL:
        case SIGFPE:
        case SIGSEGV:
        case SIGBUS:
        case SIGTRAP:
            // Record the fault address for triage by the device
            fault_addr = info->si_addr;
            if (info->si_code == SEGV_PKUERR) {
                fault_key = info->si_pkey;
            }
            // Return to the fail-safe address
            u->uc_mcontext.gregs[REG_RIP] = u->uc_mcontext.gregs[REG_R13];
            break;
        default:
            break;
    }
}

/**
 * Main.
 *
 * @return Zero if successful.
 */
int main() {
    spdlog::set_pattern("%L%C%m%dT%T%z %n: %v");
    // auto log = spdlog::stdout_color_mt("ogx");
    spdlog::flush_on(spdlog::level::debug);
    auto log = spdlog::basic_logger_mt("ogx", "/tmp/ogx.log");
    assert(log && "null logger");
    log->set_level(spdlog::level::debug);
    log->info("O(OO)GX device initializing");

    stack_t alt_stack{};
    alt_stack.ss_size = 65536;
    alt_stack.ss_sp = malloc(alt_stack.ss_size);
    assert(alt_stack.ss_sp);
    alt_stack.ss_flags = 0;
    if (sigaltstack(&alt_stack, nullptr)) {
        throw std::runtime_error("unable to set alternate stack");
    }

    struct sigaction action {};
    action.sa_handler = nullptr;
    action.sa_sigaction = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    action.sa_restorer = nullptr;
    sigaction(SIGSEGV, &action, nullptr);
    // sigset_t signals;
    // sigfillset(&signals);
    // sigprocmask(SIG_BLOCK, &signals, nullptr);

    std::unique_ptr<GuestDevice> device;

    try {
        // Load keys and flag enclave
        log->info("loading public key from {}", OGX_PUB_KEY_PATH);
        const auto ogx_pub = ReadFile(OGX_PUB_KEY_PATH);
        log->info("loading secret key from {}", OGX_SEC_KEY_PATH);
        const auto ogx_sec = ReadFile(OGX_SEC_KEY_PATH);
        log->info("loading initial enclave from {}", OGX_ENCLAVE_FLAG_PATH);
        const auto flag_enclave = ReadFile(OGX_ENCLAVE_FLAG_PATH);

        // Create the device interface
        device = std::make_unique<GuestDevice>(fault_addr, fault_key, ogx_pub, ogx_sec, OGX_FLAG_PATH);

        // Load the flag enclave
        auto e = std::make_shared<LoadEvent>(0, flag_enclave, std::vector<uint8_t>{});
        device->OnLoadEvent(ogx_pub, e.get());

        // Sandbox ourselves
        // Sandbox();
    } catch (std::exception& e) {
        log->error("unable to initialize device");
        log->error("{}", e.what());
        return 1;
    }

    try {
        // Execute the enclaves
        std::thread enclaves([&device]() {
            device->Execute();
        });

        // Handle I/O
        assert(device && "null device");
        return device->handle_IO();
    } catch (std::exception& e) {
        log->error("unable to process device events");
        log->error("{}", e.what());
        return 1;
    }
}
