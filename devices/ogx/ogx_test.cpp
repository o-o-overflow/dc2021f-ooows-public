#include <filesystem>
#include <fstream>
#include <thread>

#include "Device.h"
#include "Driver.h"
#include "Enclave.h"
#include "ThreadPool.h"
#include "gtest/gtest.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

using namespace std::chrono_literals;

// TODO: Missing tests
//       - Non-exec data
//       - Non-exec stack
//       - Non-writable code
//       - Non-writable trampoline
//       - mmap blocked
//       - mprotect blocked
//       - open* blocked

static const std::vector<uint8_t> OGX_PUB_KEY{
    0x87, 0x50, 0x3e, 0x79, 0x38, 0xf7, 0xac, 0xa0, 0x31, 0xcb, 0x9c, 0xd0, 0x3e, 0x71, 0x76, 0xe9,
    0x6a, 0xe7, 0x2c, 0x4b, 0x40, 0xa5, 0x98, 0xf5, 0x51, 0x12, 0xa8, 0xf7, 0x06, 0x4f, 0x23, 0x73,
    };
static const std::vector<uint8_t> OGX_SEC_KEY{
    0x26, 0x5c, 0xc2, 0x59, 0xa1, 0x88, 0x07, 0x55, 0x0f, 0x3a, 0x3c, 0xd2, 0x56, 0x4f, 0x00, 0x79,
    0xa8, 0x54, 0x34, 0xb4, 0xbf, 0x44, 0x11, 0x0a, 0x64, 0x07, 0x84, 0x56, 0x71, 0x63, 0xdb, 0x62,
    };
static const std::vector<uint8_t> USER_PUB_KEY{
    0x45, 0xb7, 0xbd, 0xf7, 0xa2, 0xf2, 0xc1, 0x6f, 0x38, 0xb1, 0xdd, 0xab, 0x2f, 0x58, 0x5f, 0x44,
    0x9d, 0xc3, 0x9b, 0x6c, 0xfb, 0x68, 0x37, 0xb7, 0x87, 0xad, 0x6c, 0x3e, 0x60, 0x6e, 0x2c, 0x14,
    };
static const std::vector<uint8_t> USER_SEC_KEY{
    0xd4, 0x89, 0xa5, 0xe8, 0x8f, 0x68, 0xea, 0x92, 0x43, 0xda, 0xdd, 0x29, 0x81, 0x07, 0xe3, 0x74,
    0x33, 0xa2, 0x60, 0x80, 0x42, 0xeb, 0xbe, 0x7f, 0xe9, 0x27, 0x3d, 0xf3, 0x2f, 0xbf, 0x7c, 0xbb,
    };

static const char* TEST_DEVICE_PATH = "/tmp/ogx_test.socket";
static const char* TEST_FLAG_PATH = "/tmp/FLAG";

static std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<uint8_t> data(std::istreambuf_iterator<char>(input), {});
    return data;
}

class EnclaveTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_ = spdlog::stderr_color_mt("ogx");
        log_->set_level(spdlog::level::debug);
        enclave_ = std::make_unique<Enclave>(0, TEST_FLAG_PATH);

        if (sodium_init() < 0) {
            throw std::runtime_error("unable to initialize libsodium");
        }
    }

    std::shared_ptr<spdlog::logger> log_;
    std::unique_ptr<Enclave> enclave_;
};

TEST_F(EnclaveTest, Simple) {
    // Try to load a simple payload in an enclave
    EXPECT_EQ(enclave_->id_, 0);
    EXPECT_GE(enclave_->label_, 0);
    EXPECT_NE(enclave_->code_, nullptr);
    EXPECT_EQ(reinterpret_cast<uint64_t>(enclave_->code_) & 0xfff, 0);
    EXPECT_NE(enclave_->data_, nullptr);
    EXPECT_EQ(reinterpret_cast<uint64_t>(enclave_->data_) & 0xfff, 0);
    EXPECT_NE(enclave_->stack_, nullptr);
    EXPECT_EQ(reinterpret_cast<uint64_t>(enclave_->stack_) & 0xfff, 0);
    EXPECT_NE(enclave_->trampoline_, nullptr);
    EXPECT_EQ(reinterpret_cast<uint64_t>(enclave_->trampoline_) & 0xfff, 0);

    auto result = enclave_->Execute(
        {
            // clang-format off
            // CFI label
            'B', 'B', 'Y', 'O', 'O', 'O', 'G', 'X',
            // mov rax, 3
            0xb8, 0x03, 00, 00, 00,
            0xc3,           // ret
                   // clang-format on
        },
        {'O', 'O', 'O'});
    EXPECT_EQ(result, std::vector<uint8_t>({'O', 'O', 'O'}));
}

TEST_F(EnclaveTest, DenyWRPKRU) {
    // Try to load the enclave with a WRPKRU instruction
    EXPECT_THROW(
        enclave_->Execute(
            {
                // clang-format off
                // CFI label
                'B', 'B', 'Y', 'O', 'O', 'O', 'G', 'X',
                0x0f, 0x01, 0xef,   // wrpkru
                       // clang-format on
            },
            {}),
        std::runtime_error);
}

TEST_F(EnclaveTest, AccessNonEnclaveMemory) {
    // Try to execute an enclave that accesses unauthorized memory by accessing continuation code at the call site as
    // data, which should be prevented by PKU with SIGSEGV
    EXPECT_EXIT(
        enclave_->Execute(
            {
                // clang-format off
                // CFI label
                'B', 'B', 'Y', 'O', 'O', 'O', 'G', 'X',
                0x58,               // pop rax
                0x58,               // pop rax
                0x48, 0x8b, 0x10,   // mov rdx, qword [rax]
                0x50,               // push rax
                0xc3,               // ret
                       // clang-format on
            },
            {}),
        testing::KilledBySignal(11),
        ".*");
}

TEST_F(EnclaveTest, PreventReturnEdgeROP) {
    // Try to ROP when returning from an enclave by returning to another address, which should fail at the return-edge
    // check in the trampoline with SIGILL
    EXPECT_EXIT(
        enclave_->Execute(
            {
                // clang-format off
                // CFI label
                'B', 'B', 'Y', 'O', 'O', 'O', 'G', 'X',
                // 0xcc,
                // add r8, 8
                0x49, 0x83, 0xc0, 0x08,
                // lea r9, [rsp + 0x100]
                0x4c, 0x8d, 0x8c, 0x24, 0x00, 0x01, 0x00, 0x00,
                0x4d, 0x89, 0x01,   // mov qword [r9], r8
                0x58,               // pop rax
                0x59,               // pop rcx
                // sub r9, 8
                0x49, 0x83, 0xe9, 0x08,
                0x41,               // push r9
                0x51,               // push rcx
                0x50,               // push rax
                0xc3,               // ret
                       // clang-format on
            },
            {}),
        testing::KilledBySignal(4),
        ".*");
}

TEST_F(EnclaveTest, PreventForwardEdgeROP) {
    // Check that the forward-edge check fails if the payload doesn't include the necessary tag
    EXPECT_EXIT(
        enclave_->Execute(
            {
                // clang-format off
                // mov rax, 3
                0xb8, 0x03, 00, 00, 00,
                0xc3,  // ret
                       // clang-format on
            },
            {}),
        testing::KilledBySignal(4),
        ".*");
}

TEST_F(EnclaveTest, FlagPayload) {
    // Check that the flag mapper payload works
    const auto code = ReadFile("ogx_enclave_flag.bin");
    ASSERT_EQ(code[0], 'B');
    ASSERT_EQ(code[1], 'B');
    ASSERT_EQ(code[2], 'Y');
    std::packaged_task<void()> task([&]() {
        enclave_->Execute(code, {});
    });
    auto f = task.get_future();
    std::thread task_thread(std::move(task));
    task_thread.detach();
    f.wait_for(15s);
}

TEST(ThreadPool, Scheduling) {
    auto log = spdlog::stderr_color_mt("ogx");
    log->set_level(spdlog::level::debug);

    const size_t n = 1000;
    auto pool = std::make_shared<ThreadPool>();
    pool->Init();

    std::vector<ThreadPool::Future> futures;
    for (auto i = 1; i <= n; i++) {
        futures.push_back(pool->Schedule([i]() {
            return std::vector<uint8_t>(i);
        }));
    }

    size_t sum = 0U;
    for (auto& f : futures) {
        const auto result = f.get();
        ASSERT_TRUE(!result.empty());
        sum += result.size();
    }

    EXPECT_EQ(sum, (n * (n + 1)) / 2);

    pool->Shutdown();
}

class TestDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_ = spdlog::stderr_color_mt("ogx");
        log_->set_level(spdlog::level::debug);
        std::filesystem::remove(TEST_DEVICE_PATH);
        ASSERT_FALSE(std::filesystem::exists(TEST_DEVICE_PATH));
        device_ = std::make_unique<TestDevice>(
            fault_addr_, fault_key_, OGX_PUB_KEY, OGX_SEC_KEY, TEST_FLAG_PATH, context_, TEST_DEVICE_PATH);
        ASSERT_TRUE(std::filesystem::exists(TEST_DEVICE_PATH));

        if (sodium_init() < 0) {
            throw std::runtime_error("unable to initialize libsodium");
        }

        auto code = ReadFile("ogx_enclave_flag.bin");
        std::vector<uint8_t> data{};
        load_flag_ = std::make_unique<LoadEvent>(flag_enclave_, code, data);
        code = ReadFile("ogx_enclave_signal.bin");
        load_signal_ = std::make_unique<LoadEvent>(signal_enclave_, code, data);
        std::vector<uint8_t> response{};
        response_ = std::make_unique<ResponseEvent>(flag_enclave_, response);
    }

    void TearDown() override {
        std::filesystem::remove(TEST_DEVICE_PATH);
    }

    std::shared_ptr<spdlog::logger> log_;
    boost::asio::io_context context_;
    std::unique_ptr<TestDevice> device_;
    const uint32_t flag_enclave_ = 0;
    const uint32_t signal_enclave_ = 1;
    std::unique_ptr<LoadEvent> load_flag_;
    std::unique_ptr<LoadEvent> load_signal_;
    std::unique_ptr<ResponseEvent> response_;
    void* fault_addr_;
    uint32_t fault_key_;
};

TEST_F(TestDeviceTest, EventEncryption) {
    auto load_data = load_flag_->Serialize();
    auto encrypted_load = Event::Encrypt(USER_PUB_KEY, USER_SEC_KEY, OGX_PUB_KEY, load_data);
    auto [sender, decrypted_event] = Event::Decrypt(OGX_SEC_KEY, encrypted_load);
    auto decrypted_load = std::dynamic_pointer_cast<LoadEvent>(decrypted_event);
    EXPECT_EQ(USER_PUB_KEY, sender);
    ASSERT_NE(decrypted_load, nullptr);
    EXPECT_EQ(decrypted_load->id_, load_flag_->id_);
    EXPECT_EQ(decrypted_load->code_, load_flag_->code_);
    EXPECT_EQ(decrypted_load->data_, load_flag_->data_);
}

TEST_F(TestDeviceTest, EventIO) {
    std::thread driver_thread([this]() {
        TestDriver driver(OGX_PUB_KEY, USER_PUB_KEY, USER_SEC_KEY, context_, TEST_DEVICE_PATH);
        driver.WriteEvent(load_flag_.get());

        auto read_result = driver.ReadEvent();
        ASSERT_TRUE(read_result.has_value());
        auto& [sender, event] = *read_result;
        ASSERT_NE(event, nullptr);
        auto load_event = std::dynamic_pointer_cast<LoadEvent>(event);
        ASSERT_NE(load_event, nullptr);
        EXPECT_EQ(load_event->id_, load_flag_->id_);
        EXPECT_EQ(load_event->code_, load_flag_->code_);
        EXPECT_EQ(load_event->data_, load_flag_->data_);
    });

    auto [sender, event] = device_->ReadEvent();
    ASSERT_FALSE(device_->CanReadEvent());
    ASSERT_NE(event, nullptr);
    auto load_event = std::dynamic_pointer_cast<LoadEvent>(event);
    ASSERT_NE(load_event, nullptr);
    EXPECT_EQ(load_event->id_, load_flag_->id_);
    EXPECT_EQ(load_event->code_, load_flag_->code_);
    EXPECT_EQ(load_event->data_, load_flag_->data_);

    device_->WriteEvent(USER_PUB_KEY, load_event.get());
    driver_thread.join();
}
