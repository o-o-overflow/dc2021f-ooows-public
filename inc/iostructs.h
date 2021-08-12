#ifndef IO_H_
#define IO_H_
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "vmm.h"

#define CHILD_DEVICE_CHANNEL_FD 3
#define CHILD_DEVICE_FW_MEMFD 4
#define CHILD_DEVICE_SYS_MEMFD 5
#define CHILD_DEVICE_IOAPIC_FD 6

enum {
      IOTYPE_PIO,
      IOTYPE_MMIO,
};

struct init_response {
  uint32_t magic;
  int fds[NR_MAX_VCPUS];
};

struct ioport_request {
  uint32_t port:16;
  uint32_t direction:8;
  uint32_t size:8;
  uint32_t data;
  uint32_t count;
};

struct mmio_request {
  uint64_t phys_addr;
  //uint8_t data[8];
  uint64_t data;
  uint32_t len;
  uint8_t is_write;
} __attribute__((__packed__));

struct io_request {
  uint8_t type;
  union {
    struct ioport_request ioport;
    struct mmio_request mmio;
  };
};
#endif
