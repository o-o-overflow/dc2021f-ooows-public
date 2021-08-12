#include "ogx.h"

#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>

#include "Device.h"
#include "Event.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

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
static void Sandbox() {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");
    log->info("sandboxing device");

    // TODO: Implement seccomp rules
}

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
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Zero if successful.
 */
int main(int argc, char* argv[]) {
    if (argc < 5) {
        return 1;
    }

    spdlog::set_pattern("%L%C%m%dT%T%z %n: %v");
    auto log = spdlog::stdout_color_mt("ogx");
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

    boost::asio::io_context io_context;
    std::unique_ptr<TestDevice> device;

    try {
        // Load keys and flag enclave
        log->info("loading public key from {}", argv[1]);
        const auto ogx_pub = ReadFile(argv[1]);
        log->info("loading secret key from {}", argv[2]);
        const auto ogx_sec = ReadFile(argv[2]);
        log->info("loading initial enclave from {}", argv[3]);
        const auto flag_enclave = ReadFile(argv[3]);

        // Create the device interface
        const char* test_socket_path = "/tmp/ogx_test.socket";
        std::filesystem::remove(test_socket_path);
        device = std::make_unique<TestDevice>(
            fault_addr, fault_key, ogx_pub, ogx_sec, argv[4], io_context, test_socket_path);

        // Load the flag enclave
        auto e = std::make_shared<LoadEvent>(0, flag_enclave, std::vector<uint8_t>{});
        device->ProcessLoadEvent(ogx_pub, e);

        // Sandbox ourselves
        Sandbox();
    } catch (std::exception& e) {
        log->error("unable to initialize device");
        log->error("{}", e.what());
        return 1;
    }

    // Handle device and enclave events
    try {
        assert(device && "null device");
        device->ProcessEvents();
    } catch (std::exception& e) {
        log->error("unable to process device events");
        log->error("{}", e.what());
    }

    return 0;
}
