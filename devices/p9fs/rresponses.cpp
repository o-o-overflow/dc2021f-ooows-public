#include <string.h>
#include <string>

#include "rresponses.hpp"
#include "trequests.hpp"
#include "trace.h"

#define WRITE(d, l) do {                                \
    if (offset + l > limit) throw std::exception();     \
    memcpy(data + offset, d, l);                        \
    offset += l;                                        \
  } while (0)

#define WRITEVAL(d) WRITE(&d, sizeof(d))

#define WRITESTR(str) do {                      \
    uint16_t __l = str.length();                \
    WRITEVAL(__l);                              \
    WRITE(str.c_str(), __l);                    \
  } while (0)

RResponse::RResponse(uint8_t type, uint16_t tag) : m_type(type), m_tag(tag) { }

uint32_t RResponse::SerializedSize() {
  return sizeof(uint32_t) +\
    sizeof(p9_msg_t) +\
    SerializedBodySize();
}

void RResponse::SerializeTo(uint8_t *data, size_t limit) {
  // write out the header
  size_t offset = 0;

  uint32_t total = SerializedSize();
  WRITEVAL(total);
  WRITEVAL(m_type);
  WRITEVAL(m_tag);

  SerializeBodyTo(data + offset, limit - offset);
}

RError::RError(uint16_t tag, std::string error) :
  RResponse(P9_RERROR, tag), m_error(error) { }

void RError::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITESTR(m_error);
}

uint32_t RError::SerializedBodySize() {
  return sizeof(uint16_t) +\
    m_error.length();
}

RVersion::RVersion(uint16_t tag, uint32_t msize, std::string version) :
  m_msize(msize), m_version(version), RResponse(P9_RVERSION, tag) { }

void RVersion::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_msize);
  WRITESTR(m_version);
}

uint32_t RVersion::SerializedBodySize() {
  return sizeof(uint32_t) +\
    sizeof(uint16_t) +\
    m_version.length();
}

RAuth::RAuth(uint16_t tag, qid_t aqid) : m_aqid(aqid), RResponse(P9_RAUTH, tag) { }

void RAuth::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_aqid);
}

RFlush::RFlush(uint16_t tag) : RResponse(P9_RFLUSH, tag) { }

uint32_t RAuth::SerializedBodySize() {
  return sizeof(qid_t);
}

RAttach::RAttach(uint16_t tag, qid_t qid) : m_qid(qid), RResponse(P9_RATTACH, tag) { }

void RAttach::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_qid);
}

uint32_t RAttach::SerializedBodySize() {
  return sizeof(qid_t);
}

RWalk::~RWalk() {
  TRACE_PRINT("Destructing RWalk %p", this);
  if (m_qids.size() > 2) {
    auto it = m_qids.begin();
    it++;
    auto stop = m_qids.end();
    stop--;
    for (; it != stop; it++) {
      QidObject *qobj = *it;
      delete qobj;
    }
  }
}

RWalk::RWalk(uint16_t tag, std::list<QidObject *> qobjs)
  : RResponse(P9_RWALK, tag) {
  for (auto it = qobjs.begin(); it != qobjs.end(); it++) {
    QidObject *obj = *it;
    m_qids.push_back(obj);
  }
}

void RWalk::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  TRACE_PRINT("Serializing RWalk %p", this);
  uint16_t nqids = m_qids.size();
  WRITEVAL(nqids);
  for (auto it = m_qids.begin(); it != m_qids.end(); it++) {
    qid_t q;
    QidObject *obj = *it;
    obj->Qid(&q);
    WRITEVAL(q);
  }
}

uint32_t RWalk::SerializedBodySize() {
  return sizeof(uint16_t) * sizeof(qid_t);
}

ROpen::ROpen(uint16_t tag, qid_t qid, uint32_t iounit) :
  m_qid(qid), m_iounit(iounit), RResponse(P9_ROPEN, tag) { }

void ROpen::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_qid);
  WRITEVAL(m_iounit);
}

uint32_t ROpen::SerializedBodySize() {
  return sizeof(m_qid) + sizeof(m_iounit);
}

RCreate::RCreate(uint16_t tag, qid_t qid, uint32_t iounit) :
  m_qid(qid), m_iounit(iounit), RResponse(P9_RCREATE, tag) { }

void RCreate::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_qid);
  WRITEVAL(m_iounit);
}

uint32_t RCreate::SerializedBodySize() {
  return sizeof(m_qid) + sizeof(m_iounit);
}

RRead::RRead(uint16_t tag, uint32_t count, std::unique_ptr<uint8_t[]> data) :
  m_count(count), m_data(std::move(data)), RResponse(P9_RREAD, tag) { }

void RRead::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_count);
  WRITE(m_data.get(), m_count);
}

uint32_t RRead::SerializedBodySize() {
  return sizeof(uint32_t) + m_count;
}

RWrite::RWrite(uint16_t tag, uint32_t count) :
  m_count(count), RResponse(P9_RWRITE, tag) { }

void RWrite::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITEVAL(m_count);
}

uint32_t RWrite::SerializedBodySize() {
  return sizeof(uint32_t);
}

RClunk::RClunk(uint16_t tag) : RResponse(P9_RCLUNK, tag) { }

RRemove::RRemove(uint16_t tag) : RResponse(P9_RREMOVE, tag) { }

RStat::RStat(uint16_t tag, std::unique_ptr<p9_stat_t> stat) :
  m_stat(std::move(stat)), RResponse(P9_RSTAT, tag) { }

void RStat::SerializeBodyTo(uint8_t *data, size_t limit) {
  size_t offset = 0;

  WRITE(m_stat.get(), sizeof(p9_stat_t));
}

uint32_t RStat::SerializedBodySize() {
  return sizeof(p9_stat_t);
}
