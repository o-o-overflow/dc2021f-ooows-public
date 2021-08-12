#include "ooows-net.hpp"
#include "utils/virtio.hpp"
#include "../inc/iostructs.h"
#include <unistd.h>

#define DEBUG 0

NetDev::NetDev(uint64_t mmio_start, uint32_t num_vqs) : MMIOVirtioDev(mmio_start, num_vqs) {
  m_device_id = VIRTIO_NIC;
  // just an example
  // likely will have a more well defined structure/layout
  // 2C:54:91:88:C9:E3
  uint8_t mac[] = {0x2c, 0x54, 0x91, 0x88, 0xc9, 0xe3};
  set_config_space(mac, 0x6);
  return;
}

int NetDev::handleRx(class VirtBuf *vbuf) {
  int ret, err = 0;
  // could be command, header, etc.
  uint32_t command;
  err = vbuf->readU(&command, 4);
  if (DEBUG)
    printf("got RX command: 0x%x\n", command);
  if (err)
    return ret;

  switch (command) {
    default:
      break;
  }
  return ret;
}


int NetDev::handleTx(class VirtBuf *vbuf) {
  int ret, err = 0;
  // could be command, header, etc.
  uint32_t command;
  err = vbuf->readU(&command, 4);
  if (DEBUG)
    printf("got TX command: 0x%x\n", command);
  if (err)
    return ret;

  switch (command) {
    case 0xdeadbeef:
      send_irq(2);
      break;
    default:
      break;
  }
  return ret;
}

int NetDev::got_data(uint16_t vq_idx) {
  int ret = 0;
  VirtBuf *vbuf = get_buf(vq_idx);
  if (!vbuf)
    return ret;

  if (vq_idx == VQ_DATA_TX) {
    ret = handleTx(vbuf);
  }

  else if (vq_idx == VQ_DATA_RX) {
    ret = handleRx(vbuf);
  }

  return ret;
}

int main(void) {
  int err;
  class NetDev *dev = new NetDev(MMIO_START, NUM_NET_VQS);
  err = dev->handle_IO();
  return err;
}
