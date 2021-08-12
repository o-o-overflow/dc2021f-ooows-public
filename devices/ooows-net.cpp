#include "ooows-net.hpp"
#include "utils/virtio.hpp"
#include "../inc/iostructs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "broadcooom/microengine.hpp"
#include "broadcooom/phy.hpp"
#include "broadcooom/myqueue.hpp"
#include "broadcooom/types.hpp"

#define DEBUG 0

NetDev::NetDev(uint64_t mmio_start, uint32_t num_vqs) : MMIOVirtioDev(mmio_start, num_vqs) {
  m_device_id = VIRTIO_NIC;
  set_device_features(FEATURE_CHKSUM_TX_OFFLOAD_MASK ^ FEATURE_CHKSUM_RX_OFFLOAD_MASK ^ FEATURE_PROMISC_MODE_MASK ^ FEATURE_RX_ETH_CRC_CHECK_MASK ^ FEATURE_TX_ETH_CRC_OFFLOAD_MASK);

  m_tx_ring_buf_idx = 0;
  m_rx_ring_buf_idx = 0;
  m_ram_pool_idx = 0;

  m_ram = (uint32_t*)calloc(MB(8), 1);

  m_tx_queue = new ThreadedQueue<fifo_job>();
  m_rx_queue = new ThreadedQueue<fifo_job>();
  m_pkt_in_queue = new ThreadedQueue<fifo_job>();

  m_phy = new Phy(m_tx_queue, m_rx_queue);

  instruction microcode[1024];
  int micro_fd = open("./devices-bin/net-firmware", O_RDONLY);
  if (micro_fd == -1)
  {
     printf("No net firmware.");
     exit(-1);
  }
  int size = read(micro_fd, microcode, sizeof(microcode));
  close(micro_fd);

  m_microengine = new Microengine(microcode, size, m_ram, m_tx_queue, m_rx_queue, m_pkt_in_queue);

  m_scratch = m_microengine->m_scratch;

  struct {
     uint8_t mac[6];
     uint8_t microcode[CONFIG_SPACE_MAX-6];
  } config_space;
  int fd = open("/dev/random", O_RDONLY);
  read(fd, config_space.mac, 0x6);
  close(fd);

  memcpy(config_space.microcode, m_microengine->m_code, sizeof(config_space.microcode));
  set_config_space(&config_space, sizeof(config_space));

  // m_microengine->m_ctx_ready[0] = true;
  // m_microengine->m_ctx_ready[1] = true;
  // m_microengine->m_ctx_ready[2] = true;
  // m_microengine->m_ctx_ready[3] = false;

  m_microengine->set_mac_address((char*)config_space.mac);
  m_microengine_thread = std::thread([=]{ m_microengine->interpreter_loop();});
  return;
}

int NetDev::handleRx(class VirtBuf *vbuf) {
  int ret, err = 0;
  // could be command, header, etc.

  TRACE_PRINT("got rx request len:0x%x", vbuf->m_len);

  // ensure space for the message
  if (vbuf->m_len < (MAX_MTU + sizeof(rx_msg)))
  {
     return -1;
  }

  // extract the next ready packet for the queue
  fifo_job job = m_pkt_in_queue->get();

  uint16_t size = *job.size;

  TRACE_PRINT("got rx 0x%x start %p", size, job.start);

  err = vbuf->writeU(&size, sizeof(size));
  TRACE_PRINT("wrote size %d", err);
  if (err)
  {
     return err;
  }


  err = vbuf->writeU(job.start, size);
  TRACE_PRINT("wrote data %d", err);
  if (err)
  {
     return err;
  }

  if (job.callback)
  {
     job.callback();
  }

  TRACE_PRINT("%p", (void*)vbuf->m_guest_addr);

  return 0;
}


int NetDev::handleTx(class VirtBuf *vbuf) {
  int ret, err = 0;
  // could be command, header, etc.
  tx_msg msg;
  err = vbuf->readU(&msg, sizeof(tx_msg));
  if (err)
  {
    return err;
  }

  uint16_t size = msg.size;

  TRACE_PRINT("tx size:0x%x", size);

  if (size > MAX_MTU)
  {
     return -1;
  }

  uint32_t pkt_idx;
  ram_buf_pool* pkt;

  new_pkt(&pkt_idx, &pkt);

  TRACE_PRINT("new_pkt pkt_idx:0x%x 0x%x", pkt_idx, pkt->pkt);

  // now that we have a place for the packet, copy it over
  err = vbuf->readU(pkt->pkt, size);
  if (err)
  {
     return err;
  }

  // now add this pkt to the tx_ring_buf
  err = add_to_tx_ring_buf(size, pkt_idx);
  if (err)
  {
     return err;
  }

  return ret;
}

int NetDev::handleControl(class VirtBuf *vbuf)
{
   int ret, err = 0;
   control_msg msg = {0};
   TRACE_PRINT("control msg %x", err);

   err = vbuf->readU(&msg, sizeof(msg));
   if (err)
   {
      return err;
   }

   TRACE_PRINT("control type: 0x%x data: 0x%x driver_features: 0x%x",
               msg.type,
               ((uint32_t*)msg.data)[0],
               m_driver_features);
   switch (msg.type)
   {
      case SET_MAC:
      {
         int is_changing_mac_supported = m_driver_features & FEATURE_CHANGE_MAC_MASK;
         if (is_changing_mac_supported)
         {
            ret = set_config_space(msg.data, 0x6);
            m_microengine->set_mac_address((char*) msg.data);
         }
         break;
      }

      case TOGGLE_CHKSUM_TX_OFFLOAD:
      {
         int is_chksum_tx_offload_supported = m_driver_features & FEATURE_CHKSUM_TX_OFFLOAD_MASK;
         if (is_chksum_tx_offload_supported)
         {
            m_microengine->set_chksum_tx_offload((bool) msg.data[0]);
         }
         break;
      }
      case TOGGLE_CHKSUM_RX_OFFLOAD:
      {
         int is_chksum_rx_offload_supported = m_driver_features & FEATURE_CHKSUM_RX_OFFLOAD_MASK;
         if (is_chksum_rx_offload_supported)
         {
            m_microengine->set_chksum_rx_offload((bool) msg.data[0]);
         }
         break;
      }
      case PROMISC_MODE:
      {
         int is_promisc_supported = m_driver_features & FEATURE_PROMISC_MODE_MASK;
         if (is_promisc_supported)
         {
            m_microengine->set_promisc_mode((bool) msg.data[0]);
         }
         break;
      }
      case RX_ETH_CRC_CHECK:
      {
         int is_rx_eth_crc_supported = m_driver_features & FEATURE_RX_ETH_CRC_CHECK_MASK;
         if (is_rx_eth_crc_supported)
         {
            m_microengine->set_rx_eth_crc_check_mode((bool) msg.data[0]);
         }
         break;
      }
      case TX_ETH_CRC_OFFLOAD:
      {
         int is_tx_eth_crc_offload_supported = m_driver_features & FEATURE_TX_ETH_CRC_OFFLOAD_MASK;
         TRACE_PRINT("is_tx_eth_crc_offload_supported=%d m_driver_features=0x%x mask=0x%x", is_tx_eth_crc_offload_supported, m_driver_features, FEATURE_TX_ETH_CRC_OFFLOAD_MASK);
         if (is_tx_eth_crc_offload_supported)
         {
            TRACE_PRINT("set_tx_eth_crc_offload_mode to %d", (bool) msg.data[0]);
            m_microengine->set_tx_eth_crc_offload_mode((bool) msg.data[0]);
         }
         break;
      }

   }
   return ret;
}

int NetDev::got_data(uint16_t vq_idx) {
  int ret = 0;
  VirtBuf *vbuf = get_buf(vq_idx);
  if (!vbuf)
    return ret;

  TRACE_PRINT("0x%x", vq_idx);

  if (vq_idx == VQ_CONTROL) {
     ret = handleControl(vbuf);
  }

  if (vq_idx == VQ_DATA_TX) {
    ret = handleTx(vbuf);
  }

  else if (vq_idx == VQ_DATA_RX) {
    ret = handleRx(vbuf);
  }

  ret = put_buf(vq_idx, vbuf);

  return ret;
}

int NetDev::config_space_write(uint64_t offset, uint64_t data, uint32_t size)
{
   // don't allow them to change configuration space like this,
   // they'll need to send a control message
   return -1;
}

void NetDev::new_pkt(uint32_t* ram_idx, ram_buf_pool** data)
{
   // next free pkt data is at m_ram_pool_idx in ram
   uint32_t idx = RAM_BUF_POOL + (m_ram_pool_idx * sizeof(ram_buf_pool)/4);
   ram_buf_pool* ptr = (ram_buf_pool*)(m_ram + idx);

   m_ram_pool_idx = (m_ram_pool_idx + 1) % RAM_BUF_POOL_SIZE;

   *ram_idx = idx;
   *data = ptr;
}

int NetDev::add_to_tx_ring_buf(uint16_t size, uint32_t pkt_idx)
{
   uint32_t idx = SCRATCH_TX_RING_BUF + (m_tx_ring_buf_idx * sizeof(packet_ring_buf)/4);
   packet_ring_buf* ring = (packet_ring_buf*)(m_scratch + idx);

   // if the queue is full, then we out
   if (ring->is_free_and_size & IN_USE_MASK)
   {
      TRACE_PRINT("ring 0x%x is full, skipping", idx);
      return -1;
   }

   // we ready
   ring->ram_pkt_ptr = pkt_idx;
   ring->is_free_and_size = IN_USE_MASK | (size & SIZE_MASK);
   TRACE_PRINT("pkt_ring_buf: 0x%x 0x%x", ring->is_free_and_size, ring->ram_pkt_ptr);

   // increment the buf idx
   m_tx_ring_buf_idx = (m_tx_ring_buf_idx + 1) % SCRATCH_TX_RING_BUF_SIZE;
   TRACE_PRINT("new ring_buf_idx=0x%x", m_tx_ring_buf_idx);
   return 0;
}

int main(void) {
  int err;
  class NetDev *dev = new NetDev(MMIO_START, NUM_NET_VQS);
  err = dev->handle_IO();
  return err;
}
