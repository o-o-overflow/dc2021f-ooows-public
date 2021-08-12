#include "ooows-p9fs.hpp"
#include "utils/virtio.hpp"
#include <sys/types.h>
#include "iostructs.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <thread>
#include "vmm.h"

#include "broadcooom/myqueue.hpp"
#include "p9fs/rresponses.hpp"
#include "p9fs/trequests.hpp"

P9FsDev::P9FsDev(uint64_t mmio_start, uint32_t num_vqs) : MMIOVirtioDev(mmio_start, num_vqs, (void *)HOST_SYS_MEM_VADDR) {
  m_device_id = VIRTIO_9P_TRANSPORT;

  set_config_space(NULL, 0);

  char *store = getenv("OOOWS_VM_STORE_DIR");
  if (store == NULL) {
    throw std::exception();
  }

  char *vmname = getenv("OOOWS_VM_NAME");
  if (vmname == NULL) {
    throw std::exception();
  }

  std::string mntpoint(store);
  mntpoint.append(vmname);
  mntpoint.append("/9pshare");
  TRACE_PRINT("Mountpoint: %s", mntpoint.c_str());

  struct stat st;
  if (stat(mntpoint.c_str(), &st) < 0) {
    if (mkdir(mntpoint.c_str(), S_IRWXU)) {
      throw std::exception();
    }
  }

  m_core = new P9Core(false, "share", mntpoint);
  m_trequest_queue = new ThreadedQueue<TRequest *>();
  m_rresponse_queue = new ThreadedQueue<RResponse *>();
  m_rmesgvbuf_queue = new ThreadedQueue<VirtBuf *>();

  m_trequest_thread = std::thread([&]{ RequestLoop(); });
  m_rresponse_thread = std::thread([&]{ ResponseLoop(); });
}

void P9FsDev::RequestLoop(void) {
  while(1) {
    TRequest *trequest = m_trequest_queue->get();
    RResponse *rresponse = trequest->Process(m_core);
    if (rresponse) {
      m_rresponse_queue->put(rresponse);
    }
    delete trequest;
  }
}

void P9FsDev::ResponseLoop(void) {
  while(1) {
    RResponse *rresponse = m_rresponse_queue->get();
    TRACE_PRINT("Got response %p", rresponse);
    VirtBuf *vbuf = m_rmesgvbuf_queue->get();
    TRACE_PRINT("Got virtbuf %p", vbuf);
    rresponse->SerializeTo((uint8_t *)vbuf->host_addr(0), vbuf->m_len);
    vbuf->m_nbytes_written = rresponse->SerializedSize();
    delete rresponse;
    put_buf(VQ_RMESG, vbuf);
    send_irq(P9FS_IRQ);
  }
}

// Validate an incoming 9P request and add it to the requests
// if valid
TRequest *P9FsDev::ToTRequest(p9_msg_t *msg, size_t size) {
  TRequest *ret = NULL;

  switch (msg->type) {
  case P9_TVERSION:
    ret = new TVersion(msg->tag, msg->body, size);
    break;
  case P9_TAUTH:
    ret = new TAuth(msg->tag, msg->body, size);
    break;
  case P9_TFLUSH:
    ret = new TFlush(msg->tag, msg->body, size);
    break;
  case P9_TATTACH:
    ret = new TAttach(msg->tag, msg->body, size);
    break;
  case P9_TWALK:
    ret = new TWalk(msg->tag, msg->body, size);
    break;
  case P9_TOPEN:
    ret = new TOpen(msg->tag, msg->body, size);
    break;
  case P9_TCREATE:
    ret = new TCreate(msg->tag, msg->body, size);
    break;
  case P9_TREAD:
    ret = new TRead(msg->tag, msg->body, size);
    break;
  case P9_TWRITE:
    ret = new TWrite(msg->tag, msg->body, size);
    break;
  case P9_TCLUNK:
    ret = new TClunk(msg->tag, msg->body, size);
    break;
  case P9_TREMOVE:
    ret = new TRemove(msg->tag, msg->body, size);
    break;
  case P9_TSTAT:
    ret = new TStat(msg->tag, msg->body, size);
    break;
  case P9_TWSTAT:
    ret = new TWstat(msg->tag, msg->body, size);
    break;
  }

  // invalid Trequest type
  if (ret == NULL) {
    return ret;
  }

#ifdef TRACE
  ret->show();
#endif

  // insert the Treq into the list of current requests if created
  return ret;
}

int P9FsDev::handleTMesg(class VirtBuf *vbuf)
{
  if (vbuf->m_len < sizeof(p9_pkt_t)) {
    return -1;
  }

  // CTF: bug 1 here, simple TOCTOU, will require cooperation of another vCPU
#ifndef P9PATCH1
  p9_pkt_t *pkt  = (p9_pkt_t *)vbuf->host_addr(0);

  // check that the size atleast has a type
  if (pkt->size < sizeof(p9_pkt_t)) {
    return -1;
  }

  uint8_t *raw = new uint8_t[pkt->size];
  if (!raw) {
    return 1;
  }
  memcpy(raw, vbuf->host_addr(sizeof(uint32_t)), pkt->size - 4);

  p9_msg_t *msg = (p9_msg_t *)raw;
  TRACE_PRINT("BUG1 Incoming message size: %x %x %x",
              pkt->size, msg->tag, msg->type);

  TRequest *trequest = ToTRequest(msg, pkt->size);
#else
  int err = 0;
  size_t sz = 0;
  err = vbuf->readU(&sz, sizeof(uint32_t));
  if (err) {
    return err;
  }

  if (sz < sizeof(p9_pkt_t)) {
    return 1;
  }
  sz -= sizeof(uint32_t);

  uint8_t *raw = new uint8_t[sz];
  err = vbuf->readU(raw, sz);
  if (err) {
    return err;
  }

  p9_msg_t *msg = (p9_msg_t *)raw;
  TRACE_PRINT("PATCH1 Incoming message size: %lx %x %x",
              sz, msg->tag, msg->type);

  TRequest *trequest = ToTRequest(msg, sz);
#endif

  if (trequest) {
    m_trequest_queue->put(trequest);
  }

  return 0;
}

int P9FsDev::got_data(uint16_t vq_idx) {
  int ret = 0;
  VirtBuf *vbuf = NULL;

  TRACE_PRINT("0x%x", vq_idx);

  for (vbuf = get_buf(vq_idx) ; vbuf; vbuf = get_buf(vq_idx)) {

    TRACE_PRINT("Vbuf %p", vbuf);
    if (vq_idx == VQ_TMESG) {
      ret = handleTMesg(vbuf);
      // TODO: check ret here?

      ret = put_buf(vq_idx, vbuf);
      // TODO: check ret here too??
    }

    if (vq_idx == VQ_RMESG) {
      m_rmesgvbuf_queue->put(vbuf);
    }
  }

  return ret;
}

int main(void) {
  int err;
  class P9FsDev *dev = new P9FsDev(MMIO_START, NUM_9P_VQS);
  err = dev->handle_IO();
  return err;
}
