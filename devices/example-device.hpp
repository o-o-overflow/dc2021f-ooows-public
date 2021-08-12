#pragma once
#include "utils/virtio.hpp"

#define MMIO_START 0xe1b00000
#define MMIO_END MMIO_START + 0x200
#define NUM_NET_VQS 2
#define VQ_DATA_TX 0
#define VQ_DATA_RX 1

class NetDev : public MMIOVirtioDev {
  public:
  uint32_t m_nice;

  // methods
  int got_data(uint16_t vq_idx);
  int handleTx(class VirtBuf *vbuf);
  int handleRx(class VirtBuf *vbuf);
  NetDev(uint64_t mmio_start, uint32_t num_vqs);
};
