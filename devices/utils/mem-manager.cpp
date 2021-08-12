#include "mem-manager.hpp"
#include <sys/mman.h>
#include <exception>
#include <stdexcept>
#include <string.h>


MemoryManager::MemoryManager(int fd,
                             uint64_t start_addr,
                             uint64_t size,
                             void *addr) {
  // setup our system mem
  m_memory = (char *)mmap(addr,
                      size,
                      PROT_READ|PROT_WRITE,
                      MAP_SHARED,
                      fd,
                      0);
  if (m_memory == MAP_FAILED)
    throw std::bad_alloc();
  m_guest_start_paddr = start_addr;
  m_mem_size = size;
  m_guest_end_paddr = start_addr + size;
}

MemoryManager::~MemoryManager(void) {
  munmap(m_memory, m_mem_size);
}

bool MemoryManager::oob(uint64_t guest_addr, uint64_t size) {
  if (guest_addr < m_guest_start_paddr) {
    return true;
  }
  if (guest_addr + size < m_guest_start_paddr) {
    return true;
  }

  if (guest_addr >= m_guest_end_paddr) {
    return true;
  }

  if (guest_addr + size > m_guest_end_paddr) {
    return true;
  }

  if (size > m_guest_end_paddr - m_guest_start_paddr) {
    return true;
  }

  return false;
}

void *MemoryManager::host_addr(uint64_t guest_addr) {
  return m_memory + guest_addr - m_guest_start_paddr;
}

int MemoryManager::read(uint64_t guest_addr, void * buf, uint64_t size) {
  if (oob(guest_addr, size)) {
    return -1;
  }
  memcpy(buf, host_addr(guest_addr), size);
  return 0;
}

int MemoryManager::write(uint64_t guest_addr, void *data, uint64_t size) {
  if (oob(guest_addr, size)) {
    return -1;
  }
  memcpy(host_addr(guest_addr), data, size);
  return 0;
}
