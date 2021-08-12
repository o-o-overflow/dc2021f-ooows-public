#include <sys/syscall.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>

#pragma once

const size_t PAGE_SIZE = 4096;
const size_t MAX_KEYS = 16;

/**
 * Fetch the PKRU register.
 *
 * @return Value.
 */
static inline unsigned int rdpkru() {
    unsigned int eax = 0;
    asm volatile(".byte 0x0f,0x01,0xee\n\t" : "=a"(eax) :);
    return eax;
}

/**
 * Set the PKRU register.
 *
 * @param pkru Value.
 */
static inline void wrpkru(unsigned int pkru) {
    const auto eax = pkru;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    asm volatile(".byte 0x0f,0x01,0xef\n\t" : : "a"(eax), "c"(ecx), "d"(edx));
}

/**
 * Return the rights for a protection key.
 */
static inline unsigned int ogx_pkey_get(unsigned int pkey) noexcept {
    const auto pkru = rdpkru();
    return (pkru >> (2 * pkey) & 0x3);
}

/**
 * Set rights for a protection key.
 *
 * @param pkey Key.
 * @param rights Rights.
 * @return Zero if successful.
 */
static inline unsigned int ogx_pkey_set(unsigned int pkey, unsigned int rights) noexcept {
    const auto pkru = (rights << (2 * pkey));
    wrpkru(pkru);
    return 0;
}

/**
 * Allocate a protection key.
 *
 * @return Key.
 */
static inline unsigned int ogx_pkey_alloc(unsigned int flags, unsigned int access_rights) noexcept {
    return syscall(SYS_pkey_alloc, 0, 0);
}

/**
 * Free a protection key.
 *
 * @param pkey Key.
 * @return Zero if successful.
 */
static inline unsigned int ogx_pkey_free(unsigned int pkey) noexcept {
    return syscall(SYS_pkey_free, pkey);
}

/**
 * Label memory.
 *
 * @param ptr Page-aligned pointer to memory.
 * @param size Size of region.
 * @param orig_prot Protection bits.
 * @param pkey Protection key.
 * @return Zero if successful.
 */
static inline int ogx_pkey_mprotect(void* ptr, size_t size, unsigned int orig_prot, unsigned int pkey) noexcept {
    return syscall(SYS_pkey_mprotect, ptr, size, orig_prot, pkey);
}
