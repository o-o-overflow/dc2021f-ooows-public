#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "vmm.h"
#include "apic.h"
#include "devicebus.h"
#include "iostructs.h"

extern hv_t *g_hv;
extern uint32_t g_nvcpus;
extern char **environ;

device_node_t *g_devicelist = NULL;

#define LINK_NODE(l, n) do {\
  if (l) {\
    n->next = l;\
  }\
  l = n;\
} while (0)

#define LINK_DEVICE(d) LINK_NODE(g_devicelist, d)
#define LINK_IOPORT(d, i) LINK_NODE(d->ioports, i)
#define LINK_MMIO(d, m) LINK_NODE(d->mmios, m)

device_node_t *deviceNodeForName(char *path) {
  device_node_t *cur = g_devicelist;
  for(;cur;cur=cur->next) {
    if (!strncmp(cur->path, path, strlen(cur->path)))
      return cur;
  }
  return NULL;
}

int dbusConfigFromFile(char *path) {

  FILE *config = fopen(path, "r");
  if (config == NULL) {
    perror("Failed to open device config");
    return -1;
  }

  int ret = 0;
  char device[256];
  unsigned port_start, port_len;
  unsigned mmio_start, mmio_len;
  device_node_t *devnode;
  do {
    port_start = port_len = mmio_start = mmio_len = 0;
    ret = fscanf(config, "%256s %x %x %x %x\n",
           device, &port_start, &port_len, &mmio_start, &mmio_len);

    devnode = deviceNodeForName(device);
    if (!devnode) {
      devnode = calloc(1, sizeof(device_node_t));
      devnode->path = strdup(device);
      LINK_DEVICE(devnode);
    }

    if (port_start) {
      ioport_range_t *i = calloc(1, sizeof(ioport_range_t));
      if (i == NULL) {
        perror("Failed to allocate config node");
        goto err;
      }

      i->start_port = port_start;
      i->nports = port_len;

      LINK_IOPORT(devnode, i);
    }

    if (mmio_start) {
      mmio_range_t *m = calloc(1, sizeof(mmio_range_t));
      if (m == NULL) {
        perror("Failed to allocate config node");
        goto err;
      }

      m->mmio_start = mmio_start;
      m->mmio_len = mmio_len;

      LINK_MMIO(devnode, m);
    }
  } while (ret > 0);

  return 0;

 err:
  devnode = g_devicelist;
  for(;devnode;) {
    device_node_t *next = devnode->next;
    ioport_range_t *io = devnode->ioports;
    for(;io;) {
      ioport_range_t *tmp = io->next;
      free(io);
      io = tmp;
    }

    mmio_range_t *mmio = devnode->mmios;
    for(;mmio;) {
      mmio_range_t *tmp = mmio->next;
      free(mmio);
      mmio = tmp;
    }

    free(devnode);
    devnode = next;
  }

  return -1;
}

int instantiateDevice(device_node_t *devnode) {
  char device_path[513] = {0};

  // create the socketpair to be used to comm with the device
  int i = 0;
  int sv[NR_MAX_VCPUS][2] = {0};
  for(i=0;i<g_nvcpus;i++) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv[i]) < 0) {
      return -1;
    }
  }

  pid_t child = fork();
  if (!child) {
    // close the vmm ends
    for(i=0;i<g_nvcpus;i++) {
      close(sv[i][0]);
    }

    close(g_hv->ioapic->s[0]);

    snprintf(device_path, sizeof(device_path), "%s/%s",
             DEVICE_BIN_DIR, devnode->path);

    // MUST STAY AT THE TOP. Socketpair is 3,4
    // so MEMFD will overwrite it if not dup'd first
    if (dup2(g_hv->ioapic->s[1], CHILD_DEVICE_IOAPIC_FD) < 0) {
      perror("Failed to dup ioapic socket");
    }

    // let's ensure it's a predictable fd for the child
    if (dup2(sv[0][1], CHILD_DEVICE_CHANNEL_FD) < 0) {
      perror("Failed to dup io comm socket");
    }

    if (dup2(g_hv->fw_memfd, CHILD_DEVICE_FW_MEMFD) < 0) {
      perror("Failed to dup fw memfd");
    }

    if (dup2(g_hv->sys_memfd, CHILD_DEVICE_SYS_MEMFD) < 0) {
      perror("Failed to dup sys memfd");
    }

    // TODO: we may want to setuid here and do some kind of sandboxing

    char *argv[] = {device_path, NULL};
    if (execve(device_path, argv, environ) < 0) {
      perror("Failed to exec device bin");
    }
    exit(1);
  }

  devnode->instance_pid = child;

  // wait for child to signal it's ready to receive IO
  int handshake = 0;
  if (recv(sv[0][0], &handshake, sizeof(handshake), MSG_WAITALL)
      < sizeof(handshake)) {
    perror("Failed to receive init payload\n");
    goto failed;
  }

  if (strncmp((char *)&handshake, "INIT", sizeof(handshake))) {
    fprintf(stderr, "Unexpected handshake from device\n");
    goto failed;
  }

  struct init_response response = {0};
  response.magic = *(uint32_t *)&"TINI";
  for(i=0;i<g_nvcpus;i++) {
    response.fds[i] = sv[i][1];
  }

  // now our end of the bargain
  if ((send(sv[0][0], &response, sizeof(response), 0)) < sizeof(response)) {
    perror("Failed to send tini payload\n");
    goto failed;
  }

  // close the device ends, keeping this alive until sending the opaque values
  for(i=0;i<g_nvcpus;i++) {
    close(sv[i][1]);
  }

  for(i=0;i<g_nvcpus;i++) {
    devnode->channel_fd[i] = sv[i][0];
  }

  return 0;

 failed:
  for(i=0;i<g_nvcpus;i++) {
    close(sv[i][0]);
  }
  return -1;
}

void dbusTeardown() {
  device_node_t *cur = g_devicelist;
  for(;cur;cur=cur->next) {
    pid_t device_pid = cur->instance_pid;
    if (device_pid) {
      kill(device_pid, SIGTERM);
    }
  }
}

int channelForDeviceUnlocked(vcpu_t *vcpu, device_node_t *devnode) {

  int channel_fd = devnode->channel_fd[vcpu->id];
  if (channel_fd) {
    return channel_fd;
  }

  if (instantiateDevice(devnode) < 0) {
    return -1;
  }

  return devnode->channel_fd[vcpu->id];
}

int channelForDevice(vcpu_t *vcpu, device_node_t *devnode) {
  int ret = 0;
  pthread_mutex_lock(&g_hv->bus_access_mutex);
  ret = channelForDeviceUnlocked(vcpu, devnode);
  pthread_mutex_unlock(&g_hv->bus_access_mutex);
  return ret;
}

device_node_t *deviceForPort(uint16_t port) {
  uint16_t start_port = 0;
  size_t nports = 0;
  device_node_t *cur = g_devicelist;
  for(;cur;cur=cur->next) {
    ioport_range_t *io = cur->ioports;
    for(;io;io=io->next) {
      start_port = io->start_port;
      nports = io->nports;
      if (port >= start_port && port < start_port + nports) {
        return cur;
      }
    }
  }

  return NULL;
}

device_node_t *deviceForAddr(uint64_t addr) {
  uint64_t start_addr, end_addr;
  device_node_t *cur = g_devicelist;
  for (;cur;cur=cur->next) {
    mmio_range_t *mmio = cur->mmios;
    for(;mmio;mmio=mmio->next) {
      start_addr = mmio->mmio_start;
      end_addr = start_addr + mmio->mmio_len;
      if (addr >= start_addr && addr < end_addr) {
        return cur;
      }
    }
  }

  return NULL;
}

int dbusHandlePioAccess(vcpu_t *vcpu,
                        uint16_t port,
                        uint8_t *data,
                        uint8_t direction,
                        uint8_t size,
                        uint32_t count) {
  int channel_fd = 0;
  device_node_t *devnode = NULL;;

  // iterate over the config to find which device has this registered
  devnode = deviceForPort(port);
  if (devnode == NULL) {
    return -1;
  }

  // find the channel_fd for that device, spinning up a new process for the
  // device if it does not yet exist
  channel_fd = channelForDevice(vcpu, devnode);
  if (channel_fd < 0) {
    fprintf(stderr, "Failed to retrieve channel fd in PIO\n");
  }

  // send a pio request over the wire
  struct io_request io = {
    .type = IOTYPE_PIO,
    .ioport = {
      .port = port,
      .data = *(uint32_t *)data,
      .direction = direction,
      .size = size,
      .count = count
    }
  };

  if (send(channel_fd, &io, sizeof(io), 0) < 0) {
    perror("Send IO to device");
    return -1;
  }

  // wait for ack
  int value = 0;
  if (recv(channel_fd, &value, sizeof(value), MSG_WAITALL)
      < sizeof(value)) {
    perror("Failed to receive ack");
    return -1;
  }

  // only if the direction is a read do we populate EAX
  if (direction == IO_DIRECTION_IN) {
    if (size == 1)
      *data = value & 0xff;
    else if (size == 2)
      *((uint16_t *)data) = value & 0xffff;
    else if (size == 4)
      *((uint32_t *)data) = value;
    else
      return -1;
  }

  return 0;
}

int dbusHandleMmioAccess(vcpu_t *vcpu,
                         uint64_t phys_addr,
                         uint64_t *data,
                         uint32_t len,
                         uint8_t is_write) {
  int channel_fd;
  int value = 0;
  device_node_t *devnode = NULL;

  // let's check for any type of APIC access
  // (IO OR L)
  if (apicAccess(vcpu, phys_addr)) {
    return apicMmio(vcpu, phys_addr, data, len, is_write);
  }

  // iterate over the config to find which device has this registered
  devnode = deviceForAddr(phys_addr);
  if (devnode == NULL) {
    return -1;
  }

  // find the channel_fd for that device, spinning up a new process for the
  // device if it does not yet exist
  channel_fd = channelForDevice(vcpu, devnode);
  if (channel_fd < 0) {
    fprintf(stderr, "Failed to retrieve channel fd in MMIO\n");
  }

  struct io_request io = {
    .type = IOTYPE_MMIO,
    .mmio = {
      .phys_addr = phys_addr,
      .data = *(uint32_t *)data,
      .len = len,
      .is_write = is_write
    }
  };

  if (send(channel_fd, &io, sizeof(io), 0) != sizeof(io)) {
    perror("Send MMIO to device");
    return -1;
  }

  // wait for ack / ret val
  if (recv(channel_fd, &value, sizeof(value), MSG_WAITALL)
      < sizeof(value)) {
    perror("Failed to receive ack");
    return -1;
  }

  // if it's a read, fill in kvm data
  if (!is_write) {
    if (len == 1)
      *data = value & 0xff;
    else if (len == 2)
      *((uint16_t *)data) = value & 0xffff;
    else if (len == 4)
      *((uint32_t *)data) = value;
    else
      return -1;
  }


  return 0;
}
