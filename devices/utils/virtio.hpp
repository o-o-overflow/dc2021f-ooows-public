#pragma once
#include <stdint.h>
#include <string>

#include "vmm.h"

#define MAGIC 0x74726976
#define VIRTIO_DEVICE_VERS 0x2
#define CONFIG_SPACE_MAX 0x400
#define CONFIG_SPACE_START 0x100
#define MAX_VQ_SIZE 32768

// ####################
// # Status bit masks #
// ####################
// indicates the guest OS has found the device
#define STATUS_ACKNOWLEDGE 1
// indicates the guest OS knows how to drive the device
#define STATUS_DRIVER 2
// inidivates the driver is set up and ready to drive the device
#define STATUS_DRIVER_OK 4
// indicates the driver has acknowledged all the features it understands
#define STATUS_FEATURES_OK 8
// idicates the device has experienced an unrecoverable error
#define STATUS_NEEDS_RESET 64
// indicates smth went wrong in the guest and has given up on the device
#define STATUS_FAILED 128


// #######################
// # Virtio Device Types #
// #######################
#define VIRTIO_NIC 1
#define VIRTIO_BLOCK_DEV 2
#define VIRTIO_CONSOLE 3
#define VIRTIO_ENTROPY_SOURCE 4
#define VIRTIO_MEMORY_BALLOON_TRAD 5
#define VIRTIO_IOMEMORY 6
#define VIRTIO_RPMSG 7
#define VIRTIO_SCSI_HOST 8
#define VIRTIO_9P_TRANSPORT 9
#define VIRTIO_MAC80211_WLAN 10
#define VIRTIO_RPROC_SERIAL 11
#define VIRTIO_CAIF 12
#define VIRTIO_MEMORY_BALLOON 13
#define VIRTIO_GPU_DEV 16
#define VIRTIO_TIMER_DEV 17
#define VIRTIO_INPUT_DEV 18
#define VIRTIO_SOCKET_DEV 19
#define VIRTIO_CRYPTO_DEV 20
#define VIRTIO_SIG_DIST_MODULE 21
#define VIRTIO_PSTORE_DEV 22
#define VIRTIO_IOMMU_DEV 23
#define VIRTIO_MEMORY_DEV 24
#define VIRTIO_OGX_DEV 25

// ####################
// # Register Offsets #
// ####################
#define REG_MAGIC_VAL 0x0
#define REG_DEVICE_VERS 0x4
#define REG_SUBSYS_DEV_ID 0x8
#define REG_SUBSYS_VEND_ID 0xc
#define REG_DEVICE_FEATURES 0x10
#define REG_DEVICE_FEATURES_SELECT 0x14
#define REG_DRIVER_FEATURES 0x20
#define REG_DRIVER_FEATURES_SELECT 0x24
#define REG_QUEUE_SELECT 0x30
#define REG_QUEUE_NUM_MAX 0x34
#define REG_QUEUE_NUM 0x38
#define REG_QUEUE_READY 0x44
#define REG_QUEUE_NOTIFY 0x50
#define REG_ISR 0x60
#define REG_INTR_ACK 0x64
#define REG_DEVICE_STATUS 0x70
#define REG_QUEUE_DESC_LOW 0x80
#define REG_QUEUE_DESC_HIGH 0x84
#define REG_QUEUE_DRIVER_LOW 0x90
#define REG_QUEUE_DRIVER_HIGH 0x94
#define REG_QUEUE_DEVICE_LOW 0xa0
#define REG_QUEUE_DEVICE_HIGH 0xa4
#define REG_CONFIG_GEN 0xfc
// note: 0x100+
#define REG_CONFIG_SPACE 0x100


#define MODE_R 1
#define MODE_W 2
#define MODE_RW 3

// ##################
// # Register Modes #
// ##################
#define MODE_MAGIC_VAL MODE_R
#define MODE_DEVICE_VERS MODE_R
#define MODE_SUBSYS_DEV_ID MODE_R
#define MODE_SUBSYS_VEND_ID MODE_R
#define MODE_DEVICE_FEATURES MODE_R
#define MODE_DEVICE_FEATURES_SELECT MODE_W
#define MODE_DRIVER_FEATURES MODE_W
#define MODE_DRIVER_FEATURES_SELECT MODE_W
#define MODE_QUEUE_SELECT MODE_W
#define MODE_QUEUE_NUM_MAX MODE_R
#define MODE_QUEUE_NUM MODE_W
#define MODE_QUEUE_READY MODE_RW
#define MODE_QUEUE_NOTIFY MODE_W
#define MODE_ISR MODE_R
#define MODE_INTR_ACK MODE_W
#define MODE_DEVICE_STATUS MODE_RW
#define MODE_QUEUE_DESC_LOW MODE_W
#define MODE_QUEUE_DESC_HIGH MODE_W
#define MODE_QUEUE_DRIVER_LOW MODE_W
#define MODE_QUEUE_DRIVER_HIGH MODE_W
#define MODE_QUEUE_DEVICE_LOW MODE_W
#define MODE_QUEUE_DEVICE_HIGH MODE_W
#define MODE_CONFIG_GEN MODE_R
// note: 0x100+
#define MODE_CONFIG_SPACE MODE_RW

// ###################
// # VirtqDesc flags #
// ###################
// This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT   4

typedef uint64_t guest_paddr;

union virtio_features_t {
  struct bits{
    uint32_t device_bits:24;
    uint32_t queue_and_feature_bits:14;
    uint32_t reserved:26;
  } bits;
  uint64_t full;
};

// this describes an actual buffer
struct VirtqDesc {
  // guest physical addr of buf
  // data there will be device dependent (e.g. network data has virtio headers)
  uint64_t addr;
  // length of buffer
  uint32_t len;
  // these describe the buffer (e.g. device writeable)
  uint16_t flags;
  // can chain descriptors. will be indiciated by the NEXT flag
  // this would be another desc ID
  uint16_t next;
};

// this is in the driver area
// written to by driver, read from by device
struct VirtqAvail {
  // think we should not worry about this for now
  uint16_t flags;
  // this is where the driver will put the next desc entry in the ring (mod queuesize)
  uint16_t head_idx;
  // ring buffer of desc ID's (indexes)
  uint16_t ring[];
};

// these are the elements of our used ring buffer
// id is the idx of the desc, and len is how much we wrote there as the device
struct VirtqUsedElem {
  uint32_t id;
  uint32_t len;
};

// this is in the device area
// written by device, read by driver
struct VirtqUsed {
  uint16_t flags;
  uint16_t head_idx;
  struct VirtqUsedElem ring[];
};

struct VirtQueue {
  pthread_mutex_t lock;
  uint32_t num_bufs;
  bool ready;
  bool enabled;
  uint16_t avail_tail_idx;
  uint16_t used_tail_idx;

  // desc table
  uint32_t queue_desc_low;
  uint32_t queue_desc_high;
  // avail
  uint32_t queue_driver_low;
  uint32_t queue_driver_high;
  // used
  uint32_t queue_device_low;
  uint32_t queue_device_high;
  // helpers
  uint64_t desc_table_gaddr;
  uint64_t avail_gaddr;
  uint64_t used_gaddr;
};

// helper class for devices
class VirtBuf {
  public:
  // guest paddr
  uint64_t m_guest_addr;
  // size of mem (from guest)
  uint32_t m_len;
  // desc id of buffer
  uint16_t m_id;
  // flags describing the buffer
  uint16_t flags;
  // bytes written by the device
  uint32_t m_nbytes_written;

  VirtBuf(uint64_t guest_addr, class MemoryManager *mem);
  // reads from supplied offset
  int read(uint64_t offset, void * buf, uint64_t size);
  // writes to supplied offset
  int write(uint64_t offset, void *data, uint64_t size);
  // reads and UPDATES internal head
  int readU(void * buf, uint64_t size);
  // writes and UPDATES internal head
  int writeU(void *data, uint64_t size);
  // returns the address, in host memory, of the specific offset into guest memory, for shenanigans
  void *host_addr(uint64_t offset);
  // resets head to given value, or back to start_addr if not specified
  void reset_head(uint64_t addr=0);
private:
  class MemoryManager *m_mem;
  uint64_t m_head;
};

class MMIOVirtioDev {
  public:
  // strict regs
  uint32_t m_magic;
  uint32_t m_device_version;
  uint32_t m_device_id;
  uint32_t m_vendor_id;
  uint64_t m_device_features;
  uint32_t m_device_features_sel;
  uint64_t m_driver_features;
  uint32_t m_driver_features_sel;
  // current vq index
  uint32_t m_queue_sel;
  uint64_t m_queue_notify;
  uint32_t m_isr;
  uint32_t m_isr_ack;
  uint8_t m_status;
  uint32_t m_config_gen;
  uint32_t m_config_space[CONFIG_SPACE_MAX];

  // helpers
  std::string m_dev_name;
  uint64_t m_mmio_start;
  uint64_t m_mmio_end_regs;
  uint64_t m_mmio_end;
  uint32_t m_config_space_size;

  uint32_t m_num_queues;
  struct VirtQueue **m_vqs;
  class MemoryManager *m_mem;
  pthread_mutex_t m_lock;

  // constructor
  MMIOVirtioDev(uint64_t start_addr, uint32_t num_vqs, void *host_addr = NULL);
  virtual ~MMIOVirtioDev();

  // methods
  // what devices should call from main after setting up their device
  int handle_IO(void);
  // for negotiating feature bits w/ driver
  uint32_t get_device_feature_bits(void);
  // for exposing supported features. DEVICES SHOULD USE THIS WHEN INITIALIZING
  void set_device_features(uint64_t features);
  // for devices to retreive virtbufs after receiving a notif
  VirtBuf * get_buf(uint16_t vq_idx);
  // for devices to put back used buffers
  int put_buf(uint16_t vq_idx, VirtBuf *vbuf);
  // for devives to send interrupts to the guest
  // (e.g. to notify them of buffers placed in the used ring buffer via put_buf)
  int send_irq(uint8_t irq);
  // devices SHOULD USE THIS DURING INITIALIZATION to set their config space
  int set_config_space(void *data, uint32_t size);

  // ######################################
  // # functions for devices to implement #
  // ######################################
  // This is the main entry point for most devices. MUST BE IMPLEMENTED
  virtual int got_data(uint16_t vq_idx);
  // default impl will allow writes anywhere in the space.
  // override if you need dif behavior (likely)
  virtual int config_space_write(uint64_t offset, uint64_t data, uint32_t size);
  // default impl will allow reads anywhere in the space.
  // override if you need dif behavior (less likely)
  // by default config space will be 0x400 of 0's
  virtual int config_space_read(uint64_t offset, uint64_t *out, uint32_t size);

private:
  int ready_queue(void);
  bool avail_empty(uint16_t vq_idx);
  bool used_full(uint16_t vq_idx);
  int handle_MMIO(struct mmio_request *mmio);
  int IO_loop(int fd);
  int mmio_read(uint64_t offset, uint32_t size);
  int mmio_write(uint64_t offset, uint32_t size, uint64_t data);

};
