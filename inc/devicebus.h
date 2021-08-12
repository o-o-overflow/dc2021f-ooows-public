#ifndef DEVICEBUS_H_
#define DEVICEBUS_H_

#include "vmm.h"
#include "iostructs.h"

#define DEVICE_BIN_DIR "devices-bin"

#define IO_DIRECTION_IN  0
#define IO_DIRECTION_OUT 1

typedef struct ioport_range {
  uint16_t start_port;
  uint16_t nports;
  struct ioport_range *next;
} ioport_range_t;

typedef struct mmio_range {
  uint32_t mmio_start;
  size_t mmio_len;
  struct mmio_range *next;
} mmio_range_t;

typedef struct device_node {
  char *path;
  // one channel fd per vcpu
  int channel_fd[NR_MAX_VCPUS];
  pid_t instance_pid;
  ioport_range_t *ioports;
  mmio_range_t *mmios;
  struct device_node *next;
} device_node_t;

void dbusTeardown(void);
int dbusHandlePioAccess(vcpu_t *, uint16_t, uint8_t *, uint8_t, uint8_t, uint32_t);
int dbusHandleMmioAccess(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data, uint32_t len, uint8_t is_write);
int dbusConfigFromFile(char *);
#endif
