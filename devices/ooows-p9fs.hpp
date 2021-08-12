#ifndef P9FS_DEV_H_
#define P9FS_DEV_H_

#include "broadcooom/myqueue.hpp"
#include "p9fs/rresponses.hpp"
#include "p9fs/trequests.hpp"
#include "p9fs/qidobject.hpp"
#include "utils/virtio.hpp"
#include "p9fs/p9core.hpp"
#include "p9fs/trace.h"
#include <thread>
#include <map>

#define MMIO_START 0x9b000000
#define MMIO_END   (MMIO_START + 0x200)
#define NUM_9P_VQS 2
#define VQ_TMESG 0
#define VQ_RMESG 1

#define P9FS_IRQ 9

class P9FsDev : public MMIOVirtioDev {
  private:
  P9Core *m_core;
  std::thread m_trequest_thread;
  std::thread m_rresponse_thread;
  ThreadedQueue<TRequest *> *m_trequest_queue;
  ThreadedQueue<RResponse *> *m_rresponse_queue;
  ThreadedQueue<VirtBuf *> *m_rmesgvbuf_queue;
  std::map<uint32_t, TRequest *> m_trequests_map;

  void RequestLoop();
  void ResponseLoop();
  TRequest *ToTRequest(p9_msg_t *msg, size_t size);

  public:
  int got_data(uint16_t vq_idx);
  int handleTMesg(class VirtBuf *vbuf);
  P9FsDev(uint64_t mmio_start, uint32_t num_vqs);
};

#endif
