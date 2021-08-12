#pragma once
#include "virtio_queue.h"

#define MAGIC 0x74726976
#define VIRTIO_DEVICE_VERS 0x2
#define CONFIG_SPACE_MAX 0x400

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

struct virtq * setup_virtq(uint32_t *device_start, int nbufs, uint32_t buf_sz, int qnum);
void commit_and_ready_vq(uint32_t *device_start, uint32_t vqidx, struct virtq *vq);
int add_buf(uint32_t *device, uint32_t vq_idx, struct virtq *vq, void *buf, uint32_t len);
