#pragma once
#include <stdint.h>
#include <exception>
#include <stdexcept>


class MemoryManager {
  public:
    uint64_t m_guest_start_paddr;
    uint64_t m_guest_end_paddr;
    uint64_t m_mem_size;
    char *m_memory;

    MemoryManager(int fd, uint64_t start_addr, uint64_t size, void *addr = NULL);
    ~MemoryManager();

    template <typename T>
      int readX(uint64_t guest_addr, T *out);
    template <typename T>
      int writeX(uint64_t guest_addr, T data);

    bool oob(uint64_t guest_addr, uint64_t size);
    int read(uint64_t guest_addr, void * buf, uint64_t size);
    int write(uint64_t guest_addr, void *data, uint64_t size);
    void *host_addr(uint64_t guest_addr);

};

template <typename T>
  int MemoryManager::readX(uint64_t guest_addr, T *out) {
    if (oob(guest_addr, sizeof(T))) {
      return -1;
    }
    uint64_t offset = guest_addr - m_guest_start_paddr;
    *out = ((T *)(m_memory + offset))[0];
    return 0;
  }

template <typename T>
  int MemoryManager::writeX(uint64_t guest_addr, T data) {
    if (oob(guest_addr, sizeof(T))) {
      return -1;
    }
    uint64_t offset = guest_addr - m_guest_start_paddr;
    ((T *)(m_memory+offset))[0] = data;
    return 0;
  }
