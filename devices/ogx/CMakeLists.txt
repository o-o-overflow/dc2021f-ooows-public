cmake_minimum_required(VERSION 3.10)
project(ooows_ogx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_C_FLAGS "-Wall -O3 -ggdb -pipe -fsanitize=cfi -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitize_ignorelist -fvisibility=hidden -flto -fpie -fPIC -fstack-protector-strong -Werror=format-security")
set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -O3 -ggdb -pipe -fsanitize=cfi -fsanitize-blacklist=${CMAKE_SOURCE_DIR}/sanitize_ignorelist -fvisibility=hidden -flto -fpie -fPIC -fstack-protector-strong -Werror=format-security")
set(CMAKE_EXE_LINKER_FLAGS "-flto -pie -Wl,-z,defs -Wl,-z,now -Wl,-z,relro -s")

enable_testing()
enable_language(ASM_NASM)

find_package(Boost REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/FindSodium.cmake)

include(FetchContent)
FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz)
FetchContent_MakeAvailable(googletest)

add_subdirectory(${CMAKE_SOURCE_DIR}/spdlog)
add_subdirectory(${CMAKE_SOURCE_DIR}/json)

add_executable(ogx
        ogx.cpp ogx.h
        Event.cpp Event.h
        Device.h
        Enclave.cpp Enclave.h
        ThreadPool.cpp ThreadPool.h
        trampoline.asm
        ${CMAKE_SOURCE_DIR}/../utils/virtio.cpp
        ${CMAKE_SOURCE_DIR}/../utils/mem-manager.cpp
        ${CMAKE_SOURCE_DIR}/../utils/handshake.c)
target_include_directories(ogx PUBLIC
        ${CMAKE_SOURCE_DIR}/../utils
        ${CMAKE_SOURCE_DIR}/../../inc
        ${CMAKE_SOURCE_DIR}/spdlog/include)
target_link_libraries(ogx PUBLIC spdlog nlohmann_json::nlohmann_json sodium pthread seccomp)

add_executable(ogx_test_device
        ogx_test_device.cpp ogx.h
        Event.cpp Event.h
        Device.cpp Device.h
        Enclave.cpp Enclave.h
        ThreadPool.cpp ThreadPool.h
        trampoline.asm
        ${CMAKE_SOURCE_DIR}/../utils/virtio.cpp)
target_include_directories(ogx_test_device PUBLIC
        ${CMAKE_SOURCE_DIR}/../utils
        ${CMAKE_SOURCE_DIR}/../../inc
        ${CMAKE_SOURCE_DIR}/spdlog/include)
target_link_libraries(ogx_test_device PUBLIC spdlog nlohmann_json::nlohmann_json sodium pthread)

add_executable(ogx_keys ogx_keys.cpp)
target_link_libraries(ogx_keys PUBLIC sodium)

add_custom_target(ogx_enclave_flag.bin ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -DENCLAVE -f bin -o ${CMAKE_BINARY_DIR}/ogx_enclave_flag.bin ${CMAKE_SOURCE_DIR}/ogx_enclave_flag.asm
        SOURCES ogx_enclave_flag.asm
        VERBATIM)
add_custom_target(ogx_enclave_flag ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf64 -o ${CMAKE_BINARY_DIR}/ogx_enclave_flag.o ${CMAKE_SOURCE_DIR}/ogx_enclave_flag.asm
        COMMAND ld -o ${CMAKE_BINARY_DIR}/ogx_enclave_flag ${CMAKE_BINARY_DIR}/ogx_enclave_flag.o
        SOURCES ogx_enclave_flag.asm
        VERBATIM)
add_custom_target(ogx_enclave_signal.bin ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -DENCLAVE -f bin -o ${CMAKE_BINARY_DIR}/ogx_enclave_signal.bin ${CMAKE_SOURCE_DIR}/ogx_enclave_signal.asm
        SOURCES ogx_enclave_signal.asm
        VERBATIM)
add_custom_target(ogx_enclave_signal ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf64 -o ${CMAKE_BINARY_DIR}/ogx_enclave_signal.o ${CMAKE_SOURCE_DIR}/ogx_enclave_signal.asm
        COMMAND ld -o ${CMAKE_BINARY_DIR}/ogx_enclave_signal ${CMAKE_BINARY_DIR}/ogx_enclave_signal.o
        SOURCES ogx_enclave_signal.asm
        VERBATIM)
add_custom_target(ogx_enclave_read.bin ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -DENCLAVE -f bin -o ${CMAKE_BINARY_DIR}/ogx_enclave_read.bin ${CMAKE_SOURCE_DIR}/ogx_enclave_read.asm
        SOURCES ogx_enclave_read.asm
        VERBATIM)
add_custom_target(ogx_enclave_read ALL
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf64 -o ${CMAKE_BINARY_DIR}/ogx_enclave_read.o ${CMAKE_SOURCE_DIR}/ogx_enclave_read.asm
        COMMAND ld -o ${CMAKE_BINARY_DIR}/ogx_enclave_read ${CMAKE_BINARY_DIR}/ogx_enclave_read.o
        SOURCES ogx_enclave_read.asm
        VERBATIM)

add_executable(ogx_exploit
        ogx_exploit.cpp ogx.h
        Event.cpp Event.h
        Driver.cpp Driver.h)
target_include_directories(ogx_exploit PUBLIC
        ${CMAKE_SOURCE_DIR}/../utils
        ${CMAKE_SOURCE_DIR}/../../inc
        ${CMAKE_SOURCE_DIR}/spdlog/include)
target_link_libraries(ogx_exploit PUBLIC spdlog nlohmann_json::nlohmann_json sodium pthread)

add_executable(ogx_test
        ogx_test.cpp
        Event.cpp Event.h
        Device.cpp Device.h
        Driver.cpp Driver.h
        Enclave.cpp Enclave.h
        ThreadPool.cpp ThreadPool.h
        trampoline.asm
        ${CMAKE_SOURCE_DIR}/../utils/virtio.cpp)
target_include_directories(ogx_test PUBLIC
        ${CMAKE_SOURCE_DIR}/../utils
        ${CMAKE_SOURCE_DIR}/../../inc
        ${CMAKE_SOURCE_DIR}/spdlog/include)
target_link_libraries(ogx_test PUBLIC spdlog nlohmann_json::nlohmann_json gtest_main sodium)

include(GoogleTest)
gtest_discover_tests(ogx_test)
