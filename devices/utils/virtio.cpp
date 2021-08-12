#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <thread>

#include "virtio.hpp"
#include "iostructs.h"
#include "vmm.h"
#include "mem-manager.hpp"
#include "handshake.h"

#define DEBUG 0

VirtBuf::VirtBuf(uint64_t guest_addr, class MemoryManager *mem) {
  m_head = guest_addr;
  m_mem = mem;
}

void *VirtBuf::host_addr(uint64_t offset) {
  return m_mem->host_addr(m_guest_addr+offset);
}

int VirtBuf::read(uint64_t offset, void *buf, uint64_t size) {
  int err = 0;
  err = m_mem->read(m_guest_addr+offset, buf, size);
  return err;
}

int VirtBuf::write(uint64_t offset, void *data, uint64_t size) {
  int err = 0;
  err = m_mem->write(m_guest_addr+offset, data, size);
  return err;
}

int VirtBuf::readU(void *buf, uint64_t size) {
  int err = 0;
  err = m_mem->read(m_head, buf, size);
  if (!err)
    m_head += size;
  return err;
}

int VirtBuf::writeU(void *data, uint64_t size) {
  int err = 0;
  err = m_mem->write(m_head, data, size);
  if (!err)
    m_head += size;
  return err;
}

void VirtBuf::reset_head(uint64_t addr) {
  if (!addr)
    m_head = m_guest_addr;
  else
    m_head = addr;
  return;
}

MMIOVirtioDev::MMIOVirtioDev(uint64_t mmio_start,
                             uint32_t num_vqs,
                             void *host_addr) {
  // allocate our virtqueue array
  m_vqs = (struct VirtQueue **)calloc(num_vqs, sizeof(struct VirtQueue *));
  if (!m_vqs)
    throw std::bad_alloc();
  int i;
  for (i=0; i < num_vqs; i++) {
    m_vqs[i] = (struct VirtQueue *)calloc(1, sizeof(struct VirtQueue));
    if (!m_vqs[i])
      throw std::bad_alloc();
  }
  pthread_mutex_init(&m_lock, NULL);
  m_num_queues = num_vqs;
  m_mmio_start = mmio_start;
  m_mmio_end_regs = mmio_start + CONFIG_SPACE_START;
  // regs
  m_status = 0;
  m_magic = MAGIC;
  m_device_version = VIRTIO_DEVICE_VERS;
  m_device_features = 0;
  m_device_features_sel = 0;
  m_driver_features = 0;
  m_driver_features_sel = 0;
  m_queue_sel = 0;
  m_queue_notify = 0;
  m_isr = 0;
  m_isr_ack = 0;
  m_status = STATUS_ACKNOWLEDGE;
  m_config_gen = 0;
  memset(m_config_space, 0, CONFIG_SPACE_MAX);
  m_config_space_size = CONFIG_SPACE_MAX;
  // setup our memory manager
  m_mem = new MemoryManager(CHILD_DEVICE_SYS_MEMFD,
                            GUEST_SYS_MEM_PADDR,
                            HOST_SYS_MEM_SIZE,
                            host_addr);
}

MMIOVirtioDev::~MMIOVirtioDev(void) {
  delete(m_mem);

  if (m_num_queues > 0) {
    int i;
    for (i=0; i < m_num_queues; i++) {
      free(m_vqs[i]);
    }
    free(m_vqs);
  }
}

int MMIOVirtioDev::got_data(uint16_t vq_idx) {
  printf("override me.\n");
  return 0;
}

int MMIOVirtioDev::set_config_space(void *data, uint32_t size) {
  if (size > CONFIG_SPACE_MAX)
    return -1;
  memcpy(m_config_space, data, size);
  m_config_space_size = size;
  return 0;
}

int MMIOVirtioDev::send_irq(uint8_t irq) {
  int err = 0;
  err = write(CHILD_DEVICE_IOAPIC_FD, &irq, sizeof(irq));
  if (err < 0)
    perror("write");
  if (err != sizeof(irq))
    err = -1;
  return err;
}

int MMIOVirtioDev::handle_MMIO(struct mmio_request *mmio) {
  int ret = 0;
  uint64_t offset = mmio->phys_addr - m_mmio_start;

  pthread_mutex_lock(&m_lock);
  if (mmio->is_write) {
    mmio_write(offset, mmio->len, mmio->data);
  }
  else {
    ret = mmio_read(offset, mmio->len);
  }
  pthread_mutex_unlock(&m_lock);

  return ret;
}

int MMIOVirtioDev::IO_loop(int fd) {
  int err = 0;

  struct io_request io = {0};
  while (read(fd, &io, sizeof(io)) == sizeof(io)) {
    switch(io.type) {
      case IOTYPE_MMIO:
        err = handle_MMIO(&io.mmio);
        HandledRequest(fd, err);
        break;
      default:
        fprintf(stderr, "Unsupported IO type encountered: %d\n", io.type);
        return -1;
    }

    if (err) return err;
  }

  return err;
}

int MMIOVirtioDev::handle_IO(void) {
  size_t nvcpus;
  int vcpu_fds[4];
  int err = 0;
  // handshake first
  err = DeviceHandshake(CHILD_DEVICE_CHANNEL_FD, (int *)&vcpu_fds, &nvcpus);
  if (err != 0) {
    printf("Handshake failed\n");
    return -1;
  }

  // process requests in worker loops
  // TODO uncomment when virtio is protected against race conditions
  int i = 0;
  std::thread *workers = new std::thread[NR_MAX_VCPUS];
  for(i=0;i<nvcpus;i++) {
    workers[i] = std::thread([this] (int fd) {
                               IO_loop(fd);
                             }, vcpu_fds[i]);
  }

  for(i=0;i<nvcpus;i++) {
    workers[i].join();
  }

  delete[] workers;
  return err;
}

int MMIOVirtioDev::config_space_read(uint64_t offset,
    uint64_t *out,
    uint32_t size) {
  offset -= CONFIG_SPACE_START;
  memcpy(out, ((uint8_t *)m_config_space)+offset, size);
  return 0;
}

int MMIOVirtioDev::config_space_write(uint64_t offset,
    uint64_t data,
    uint32_t size) {
  offset -= CONFIG_SPACE_START;
  memcpy(((uint8_t *)m_config_space)+offset, &data, size);
  return 0;
}

void MMIOVirtioDev::set_device_features(uint64_t features) {
  m_device_features = features;
}

uint32_t MMIOVirtioDev::get_device_feature_bits(void) {
  // high bit is reserved
  uint32_t features;

  // if device features select is 0, we return 0-31
  if (m_device_features_sel)
    features = m_device_features & 0xffffffff;

  // if it's 1, we return 32-63
  else
    features = ((m_device_features) >> 32) & 0xffffffff;

  return features;
}

int MMIOVirtioDev::mmio_read(uint64_t offset, uint32_t size) {

  // check if it's a config space read
  if (offset >= CONFIG_SPACE_START
      && (offset+size) <= (CONFIG_SPACE_START+m_config_space_size)) {
    uint64_t val = 0;
    config_space_read(offset, &val, size);
    return val;
  }


  switch(offset) {
    case REG_MAGIC_VAL:
      // 0x74726976 "virt"
      return m_magic;
      break;
    case REG_DEVICE_VERS:
      // 0x2
      return m_device_version;
      break;
    case REG_SUBSYS_DEV_ID:
      return m_device_id;
      break;
    case REG_SUBSYS_VEND_ID:
      return m_vendor_id;
      break;
    case REG_DEVICE_FEATURES:
      // NOTE: uint32_t conversion
      return get_device_feature_bits();
      break;
    case REG_QUEUE_NUM_MAX:
      if (m_vqs[m_queue_sel]->ready)
        return MAX_VQ_SIZE;
      return 0;
      break;
    case REG_QUEUE_READY:
      return m_vqs[m_queue_sel]->ready;
      break;
    case REG_ISR:
      return m_isr;
      break;
    case REG_DEVICE_STATUS:
      return m_status;
      break;
  }

  return 0;
}

int MMIOVirtioDev::mmio_write(uint64_t offset, uint32_t size, uint64_t data) {
  int ret = 0;
  int err = 0;

  // check if it's a config space write
  if (offset >= CONFIG_SPACE_START
      && (offset+size) <= (CONFIG_SPACE_START + m_config_space_size)) {
    return config_space_write(offset, data, size);
  }


  // TODO: Enforce reads/writes of len 4 (32bit),
  // unless reading/writing config space(?)
  switch(offset) {
    case REG_DEVICE_FEATURES_SELECT:
      if (data)
        m_device_features_sel = 1;
      else
        m_device_features_sel = 0;
      break;
    case REG_DRIVER_FEATURES:
      // if m_driver_features_sel is 1, set bits 32-63
      if (m_driver_features_sel) {
        m_driver_features &= 0x00000000ffffffff;
        m_driver_features |= ((data & 0xffffffff) << 32);
      }
      // else, set bits 0-31
      else {
        m_driver_features &= 0xffffffff00000000;
        m_driver_features |= (data & 0xffffffff);
      }
      if (DEBUG) {
         printf("m_driver_features_sel=0x%x\n", m_driver_features_sel);
      }
      break;
    case REG_DRIVER_FEATURES_SELECT:
      if (data)
        m_driver_features_sel = 1;
      else
        m_driver_features_sel = 0;
      break;
    case REG_QUEUE_SELECT:
      if (data < m_num_queues)
        m_queue_sel = data;
      break;
    // this is how many buffers are in our desc table
    case REG_QUEUE_NUM:
      if (data < MAX_VQ_SIZE) {
        // driver cannot change num bufs when the queue is ready
        if (m_vqs[m_queue_sel]->ready == false)
          m_vqs[m_queue_sel]->num_bufs = data;
      }
      break;
    case REG_QUEUE_READY:
      if (data) {
        // TODO: Check return val and notify device if good (0)
        err = ready_queue();
        if (err == 0) {
          ret = got_data(m_queue_sel);
        }
      }
      else {
        // unready queue TODO
      }
      break;
    case REG_QUEUE_NOTIFY:
      ret = got_data(data);
      break;
    case REG_INTR_ACK:
      m_isr_ack = data;
      break;
    case REG_QUEUE_DESC_LOW:
      m_vqs[m_queue_sel]->queue_desc_low = data;
      break;
    case REG_QUEUE_DESC_HIGH:
      m_vqs[m_queue_sel]->queue_desc_high = data;
      break;
    case REG_QUEUE_DRIVER_LOW:
      m_vqs[m_queue_sel]->queue_driver_low = data;
      break;
    case REG_QUEUE_DRIVER_HIGH:
      m_vqs[m_queue_sel]->queue_driver_high = data;
      break;
    case REG_QUEUE_DEVICE_LOW:
      m_vqs[m_queue_sel]->queue_device_low = data;
      break;
    case REG_QUEUE_DEVICE_HIGH:
      m_vqs[m_queue_sel]->queue_device_high = data;
      break;
  }

  return 0;
}

int MMIOVirtioDev::ready_queue(void) {
  // check if num bufs has been set
  if (m_vqs[m_queue_sel]->num_bufs == 0)
    return -1;

  uint64_t desc_table_addr = ((uint64_t)m_vqs[m_queue_sel]->queue_desc_high << 32)
    | (m_vqs[m_queue_sel]->queue_desc_low);
  uint64_t avail_addr = ((uint64_t)m_vqs[m_queue_sel]->queue_driver_high << 32)
    | (m_vqs[m_queue_sel]->queue_driver_low);
  uint64_t used_addr = ((uint64_t)m_vqs[m_queue_sel]->queue_device_high << 32)
    | (m_vqs[m_queue_sel]->queue_device_low);

  if (DEBUG) {
    printf("desc_table_addr: 0x%lx\n", desc_table_addr);
    printf("avail_addr: 0x%lx\n", avail_addr);
    printf("used_addr: 0x%lx\n", used_addr);
  }

  uint32_t num_bufs = m_vqs[m_queue_sel]->num_bufs;
  // do bounds checks on all the spaces
  uint64_t desc_table_sz = sizeof(struct VirtqDesc) * num_bufs;
  if (m_mem->oob(desc_table_addr, desc_table_sz))
    return -1;

  uint64_t avail_sz = sizeof(struct VirtqAvail)
    + (num_bufs * sizeof(uint16_t));
  if (m_mem->oob(avail_addr, avail_sz))
    return -1;

  uint64_t used_sz = sizeof(struct VirtqUsed)
    + (num_bufs * sizeof(struct VirtqUsedElem));
  if (m_mem->oob(used_addr, used_sz))
    return -1;

  m_vqs[m_queue_sel]->desc_table_gaddr = desc_table_addr;
  m_vqs[m_queue_sel]->avail_gaddr = avail_addr;
  m_vqs[m_queue_sel]->used_gaddr = used_addr;
  m_vqs[m_queue_sel]->ready = true;

  return 0;
}

// NOTE: assumes the queue is ready
bool MMIOVirtioDev::avail_empty(uint16_t vq_idx) {
  struct VirtQueue *vq = m_vqs[vq_idx];
  int err = 0;
  uint16_t avail_head_idx;
  err = m_mem->readX<uint16_t>(vq->avail_gaddr
      + offsetof(struct VirtqAvail, head_idx),
      &avail_head_idx);

  // figure out a better solution for error handling here
  if (err != 0)
    throw std::out_of_range("");

  // if avail_head_idx == tail_idx, we're empty
  //printf("head_idx: %d    vq->avail_tail_idx: %d\n", head_idx, vq->avail_tail_idx);
  return avail_head_idx == vq->avail_tail_idx;
}

// NOTE: assumes the queue is ready
bool MMIOVirtioDev::used_full(uint16_t vq_idx) {
  struct VirtQueue *vq = m_vqs[vq_idx];
  int err = 0;
  uint16_t used_head_idx;
  err = m_mem->readX<uint16_t>(vq->used_gaddr
      + offsetof(struct VirtqUsed, head_idx),
      &used_head_idx);

  // figure out a better solution for error handling here
  if (err != 0)
    throw std::out_of_range("");

  // if used_head_idx == avail_tail we've given back all the bufs we've consumed
  //printf("used_head_idx: %d    vq->avail_tail_idx: %d\n", used_head_idx, vq->avail_tail_idx);
  return used_head_idx == vq->avail_tail_idx;
}

VirtBuf * MMIOVirtioDev::get_buf(uint16_t vq_idx) {
  if (vq_idx >= m_num_queues)
    return NULL;
  // first check if the currently selected vq is ready
  if (!m_vqs[vq_idx]->ready)
    return NULL;

  // check if our available ring buffer is empty
  if (avail_empty(vq_idx))
    return NULL;

  int err = 0;
  struct VirtQueue *vq = m_vqs[vq_idx];
  uint16_t avail_tail_idx = vq->avail_tail_idx;
  // mod before indexing for wrap
  avail_tail_idx %= vq->num_bufs;
  uint64_t avail_addr = vq->avail_gaddr;
  // avail
  //   ...
  //   ring [
  //     ...      0
  //     ...      ...
  //       id     avail_tail_idx
  //     ...      ...
  //   ]
  uint16_t desc_id;
  err = m_mem->readX<uint16_t>(avail_addr
      + offsetof(struct VirtqAvail, ring)
      + sizeof(uint16_t)*avail_tail_idx,
      &desc_id);

  if (err != 0)
    return NULL;
  // check their desc_id against the number of buffers they say are available
  if (desc_id > vq->num_bufs)
    return NULL;
  // now lets pull out the proper descriptor
  uint64_t desc_table_addr = vq->desc_table_gaddr;
  struct VirtqDesc desc;
  err = m_mem->read(desc_table_addr
      + (sizeof(struct VirtqDesc)*desc_id),
      &desc,
      sizeof(struct VirtqDesc));

  if (err != 0)
    return NULL;

  // do a bounds check on the buffer they handed us to ensure it's
  // within the system memory
  if (m_mem->oob(desc.addr, desc.len)) {
    return NULL;
  }
  // TODO: Handle chaining
  VirtBuf *vbuf = new VirtBuf(desc.addr, m_mem);
  vbuf->m_guest_addr = desc.addr;
  vbuf->m_len = desc.len;
  vbuf->flags = desc.flags;
  vbuf->m_id = desc_id;

  // finally, increment the avail tail idx
  vq->avail_tail_idx += 1;

  return vbuf;
}

int MMIOVirtioDev::put_buf(uint16_t vq_idx, VirtBuf *vbuf) {
  if (vq_idx >= m_num_queues)
    return -1;
  // first check if the currently selected vq is ready
  if (!m_vqs[vq_idx]->ready)
    return -1;

  // check if our used ring buffer is full
  // this shouldn't actually be possible because we can't return more buffers
  // than we've been given
  if (used_full(vq_idx))
    return -1;

  int err = 0;
  struct VirtQueue *vq = m_vqs[vq_idx];
  struct VirtqUsedElem used_elem;
  used_elem.id = vbuf->m_id;
  used_elem.len = vbuf->m_nbytes_written;

  // get our head idx which is in guest mem but we're responsible for updating
  uint16_t head_idx;
  err = m_mem->readX<uint16_t>(vq->used_gaddr
      + offsetof(struct VirtqUsed, head_idx),
      &head_idx);

  if (err !=0 )
    return -1;
  // inc new head before we mod
  uint16_t new_head_idx = head_idx + 1;
  // mod before indexing for wrap
  head_idx %= vq->num_bufs;
  if (DEBUG) {
    printf("Placing buf of id %d and addr 0x%lx at used idx %d\n",
        vbuf->m_id, vbuf->m_guest_addr, head_idx);
  }
  // so now we write out filled in used_elem to the proper idx in the used_ring
  uint64_t addr_to_write = vq->used_gaddr
    + offsetof(struct VirtqUsed, ring)
    + sizeof(struct VirtqUsedElem)*vq->num_bufs;

  err = m_mem->write(addr_to_write,
      (void *)&used_elem,
      sizeof(struct VirtqUsedElem));

  if (err != 0)
    return -1;

  // finally update the head idx
  err = m_mem->writeX<uint16_t>(vq->used_gaddr
      + offsetof(struct VirtqUsed, head_idx),
      new_head_idx);

  if (err != 0)
    return -1;

  return 0;
}
