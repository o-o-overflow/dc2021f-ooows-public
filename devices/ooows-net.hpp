#pragma once
#include <thread>

#include "utils/virtio.hpp"

#include "broadcooom/microengine.hpp"
#include "broadcooom/phy.hpp"
#include "broadcooom/myqueue.hpp"
#include "broadcooom/types.hpp"

#define MMIO_START 0xe1b00000
#define MMIO_END MMIO_START + 0x200
#define NUM_NET_VQS 3
#define VQ_CONTROL 0
#define VQ_DATA_TX 1
#define VQ_DATA_RX 2

#define SCRATCH_TX_PACKETS 0
#define SCRATCH_RX_PACKETS 1
#define SCRATCH_TX_RING_BUF 2
#define SCRATCH_TX_RING_BUF_SIZE 16
#define SCRATCH_RX_RING_BUF 34
#define SCRATCH_RX_RING_BUF_SIZE 16

#define MAX_MTU 1518


#define RAM_BUF_POOL 0x24000
#define RAM_BUF_POOL_SIZE 256


typedef struct {
   char pkt[1520];
} ram_buf_pool;

#define IN_USE_MASK 0x80000000
#define SIZE_MASK 0xFFFFFF

typedef struct {
   uint32_t is_free_and_size;
   uint32_t ram_pkt_ptr;
} packet_ring_buf;



#define FEATURE_CHKSUM_TX_OFFLOAD_MASK 0x1
#define FEATURE_CHKSUM_RX_OFFLOAD_MASK 0x1<<1
#define FEATURE_PROMISC_MODE_MASK 0x1<<2
#define FEATURE_CHANGE_MAC_MASK 0x1<<3
#define FEATURE_RX_ETH_CRC_CHECK_MASK 0x1<<4
#define FEATURE_TX_ETH_CRC_OFFLOAD_MASK 0x1<<5

// possible control messages
#define SET_MAC 0
#define TOGGLE_CHKSUM_TX_OFFLOAD 1
#define TOGGLE_CHKSUM_RX_OFFLOAD 2
#define PROMISC_MODE 3
#define RX_ETH_CRC_CHECK 4
#define TX_ETH_CRC_OFFLOAD 5

typedef struct {
   uint8_t type;
   char data[128];
} control_msg;

typedef struct __attribute__((packed)) {
   uint16_t size;
   char data[];
} tx_msg;

typedef struct __attribute__((packed)) {
   uint16_t size;
   char data[];
} rx_msg;


extern unsigned char devices_broadcooom_engine_out[];
extern unsigned int devices_broadcooom_engine_out_len;

class NetDev : public MMIOVirtioDev {
  private:
  uint16_t m_tx_ring_buf_idx;
  uint16_t m_rx_ring_buf_idx;
  uint16_t m_ram_pool_idx;

  // Allocate space in RAM for a new pkt, return the ram_idx and a pointer to the data
  void new_pkt(uint32_t* ram_idx, ram_buf_pool** data);
  int add_to_tx_ring_buf(uint16_t size, uint32_t idx);

  public:
  uint32_t m_nice;
  uint32_t* m_scratch;
  uint32_t* m_ram;
  ThreadedQueue<fifo_job>* m_tx_queue;
  ThreadedQueue<fifo_job>* m_rx_queue;
  ThreadedQueue<fifo_job>* m_pkt_in_queue;
  Phy* m_phy;
  Microengine* m_microengine;
  std::thread m_microengine_thread;

  // methods
  int got_data(uint16_t vq_idx);
  int handleTx(class VirtBuf *vbuf);
  int handleRx(class VirtBuf *vbuf);
  int handleControl(class VirtBuf *vbuf);
  NetDev(uint64_t mmio_start, uint32_t num_vqs);

  // overwrite parent
  int config_space_write(uint64_t offset, uint64_t data, uint32_t size);
};
