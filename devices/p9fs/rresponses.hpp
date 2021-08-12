#ifndef P9_RRESPONSE_H_
#define P9_RRESPONSE_H_

#include "qidobject.hpp"
#include <list>

class RResponse {
  protected:
  uint8_t m_type;
  uint16_t m_tag;

  public:
  virtual ~RResponse() = default;
  RResponse(uint8_t type, uint16_t tag);
  uint32_t SerializedSize();
  void SerializeTo(uint8_t *data, size_t limit);
  virtual void SerializeBodyTo(uint8_t *data, size_t limit) { return; }
  virtual uint32_t SerializedBodySize() { return 0; }
};

class RError : public RResponse {
  private:
  std::string m_error;

  public:
  RError(uint16_t tag, std::string error);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RVersion : public RResponse {
  private:
  uint32_t m_msize;
  std::string m_version;

  public:
  RVersion(uint16_t tag, uint32_t msize, std::string version);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RAuth : public RResponse {
  private:
  qid_t m_aqid;

  public:
  RAuth(uint16_t tag, qid_t aqid);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RFlush : public RResponse {
  public:
  RFlush(uint16_t tag);
};

class RAttach : public RResponse {
  private:
  qid_t m_qid;

  public:
  RAttach(uint16_t tag, qid_t qid);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RWalk : public RResponse {
  private:
  std::list<QidObject *> m_qids;

  public:
  ~RWalk();
  RWalk(uint16_t tag, std::list<QidObject *> qobjs);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class ROpen : public RResponse {
  private:
  qid_t m_qid;
  uint32_t m_iounit;

  public:
  ROpen(uint16_t tag, qid_t qid, uint32_t iounit = 0);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RCreate : public RResponse {
  private:
  uint32_t m_iounit;
  qid_t m_qid;

  public:
  RCreate(uint16_t tag, qid_t qid, uint32_t iounit = 0);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RRead : public RResponse {
  private:
  uint32_t m_count;
  std::unique_ptr<uint8_t[]> m_data;

  public:
  RRead(uint16_t tag, uint32_t count, std::unique_ptr<uint8_t[]> data);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RWrite : public RResponse {
  private:
  uint32_t m_count;

  public:
  RWrite(uint16_t tag, uint32_t count);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};

class RClunk : public RResponse {
  public:
  RClunk(uint16_t tag);
};

class RRemove : public RResponse {
  public:
  RRemove(uint16_t tag);
};

class RStat : public RResponse {
  private:
  std::unique_ptr<p9_stat_t> m_stat;

  public:
  RStat(uint16_t tag, std::unique_ptr<p9_stat_t> stat);
  void SerializeBodyTo(uint8_t *data, size_t limit);
  uint32_t SerializedBodySize();
};
#endif
