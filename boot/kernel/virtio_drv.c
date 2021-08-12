#include "virtio_queue.h"
#include "virtio_drv.h"
#include "malloc.h"

//extern char *heap;

struct virtq * setup_virtq(uint32_t *device, int nbufs, uint32_t buf_sz, int qnum) {
  struct virtq *vq = (struct virtq *)malloc(sizeof(struct virtq));
  struct virtq_desc *desc_table = (struct virtq_desc *)malloc(sizeof(struct virtq_desc)*nbufs);
  struct virtq_avail *avail = (struct virtq_avail *)malloc(sizeof(struct virtq_avail) + sizeof(uint16_t[nbufs]));
  struct virtq_used *used = (struct virtq_used *)malloc(sizeof(struct virtq_used) + sizeof(struct virtq_used_elem[nbufs]));
  int i;
  for (i=0; i < nbufs; i++) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wint-conversion"
    desc_table[i].addr = malloc(buf_sz);
    #pragma GCC diagnostic pop
    if (desc_table[i].addr == 0)
      return NULL;
    desc_table[i].len = buf_sz;
    avail->ring[i] = i;
    avail->idx++;
  }

  vq->num = nbufs;
  vq->desc = desc_table;
  vq->avail = avail;
  vq->used = used;

  // lets select the proper queue
  device[REG_QUEUE_SELECT/4] = (uint32_t)qnum;
  // tell the device how many buffers in this vq
  device[REG_QUEUE_NUM/4] = nbufs;

  return vq;
}

void commit_and_ready_vq(uint32_t *device, uint32_t vq_idx, struct virtq *vq) {
  // lets select the proper queue
  device[REG_QUEUE_SELECT/4] = (uint32_t) vq_idx;

  // now write our addresses to the virtio nic
  device[REG_QUEUE_DESC_LOW/4] = (uint32_t)vq->desc;
  device[REG_QUEUE_DRIVER_LOW/4] = (uint32_t)vq->avail;
  device[REG_QUEUE_DEVICE_LOW/4] = (uint32_t)vq->used;

  // let's signal virtq 0 is ready
  device[REG_QUEUE_READY/4] = 1;
  return;
}

int add_buf(uint32_t *device, uint32_t vq_idx, struct virtq *vq, void *buf, uint32_t len) {
  uint16_t desc_idx = vq->avail->idx;
  desc_idx %= vq->num;
  vq->desc[desc_idx].addr = (uint64_t)buf;
  vq->desc[desc_idx].len = len;
  vq->avail->ring[desc_idx] = desc_idx;
  // increment the avail head idx
  vq->avail->idx++;
  // notify the device of the index we just wrote
  device[REG_QUEUE_NOTIFY/4] = vq_idx;
  return 0;
}
