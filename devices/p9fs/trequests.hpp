#ifndef P9_TREQUESTS_H_
#define P9_TREQUESTS_H_

#include "rresponses.hpp"
#include "iostructs.h"
#include "p9core.hpp"
#include <unistd.h>
#include <memory>
#include <string>
#include <list>

enum {
    P9_TVERSION = 100,
    P9_RVERSION,
    P9_TAUTH = 102,
    P9_RAUTH,
    P9_TATTACH = 104,
    P9_RATTACH,
    P9_TERROR = 106,
    P9_RERROR,
    P9_TFLUSH = 108,
    P9_RFLUSH,
    P9_TWALK = 110,
    P9_RWALK,
    P9_TOPEN = 112,
    P9_ROPEN,
    P9_TCREATE = 114,
    P9_RCREATE,
    P9_TREAD = 116,
    P9_RREAD,
    P9_TWRITE = 118,
    P9_RWRITE,
    P9_TCLUNK = 120,
    P9_RCLUNK,
    P9_TREMOVE = 122,
    P9_RREMOVE,
    P9_TSTAT = 124,
    P9_RSTAT,
    P9_TWSTAT = 126,
    P9_RWSTAT,
};

typedef struct  __attribute__((packed)) {
  uint8_t type;
  uint16_t tag;
  uint8_t body[];
} p9_msg_t;

typedef struct __attribute__((packed)) {
  uint32_t size;
  p9_msg_t msg;
} p9_pkt_t;

class TRequest {
  protected:
  uint8_t m_type;
  uint16_t m_tag;

  bool m_completed = true;
  bool m_errored = false;
  bool m_cancelled = false;
  std::string m_error;

  public:
  TRequest(uint8_t type, uint16_t tag);
  void SetError(std::string error);

  uint8_t Type() { return m_type; }
  uint16_t Tag() { return m_tag; }
  RResponse *Process(P9Core *core);
  RResponse *GenerateError();

  virtual void show() { return; }
  virtual void Cancel() { m_cancelled = true; }
  virtual bool Valid() { return true; }
  virtual bool Execute(P9Core *core) { return false; }
  virtual RResponse *Respond() { return NULL; }
};

class TVersion : public TRequest {
  private:
  uint32_t m_msize;
  std::string m_version;

public:
  TVersion(uint16_t tag, uint8_t *body, size_t size);
  void show(void);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TAuth : public TRequest {
  private:
  uint32_t m_afid;
  std::string m_uname;
  std::string m_aname;

  qid_t m_aqid;

  public:
  TAuth(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  void show(void);
  RResponse *Respond();
};

class TFlush : public TRequest {
  private:
  uint32_t m_oldtag;

  public:
  TFlush(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TAttach : public TRequest {
  private:
  uint32_t m_fid;
  uint32_t m_afid;
  std::string m_uname;
  std::string m_aname;

  // out params
  qid_t m_qid;

  public:
  TAttach(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TWalk : public TRequest {
  private:
  uint32_t m_fid;
  uint32_t m_newfid;
  std::list<std::string> m_wnames;

  std::list<QidObject *> m_qids;

  public:
  TWalk(uint16_t tag, uint8_t *body, size_t size);
  void show(void);
  bool Valid();
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TOpen : public TRequest {
  private:
  uint32_t m_fid;
  uint8_t m_mode;

  qid_t m_qid;

  public:
  TOpen(uint16_t tag, uint8_t *body, size_t size);
  void show(void);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TCreate : public TRequest {
  private:
  uint32_t m_fid;
  std::string m_name;
  uint32_t m_perm;
  uint8_t m_mode;

  qid_t m_qid;

  public:
  TCreate(uint16_t tag, uint8_t *body, size_t size);
  void show(void);
  bool Valid();
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TRead : public TRequest {
  private:
  uint32_t m_fid;
  uint64_t m_offset;
  uint32_t m_count;

  std::unique_ptr<uint8_t[]> m_data;

  public:
  TRead(uint16_t tag, uint8_t *body, size_t size);
  void show(void);
  bool Execute(P9Core *core);
  RResponse *Respond(void);
};

class TWrite : public TRequest {
  private:
  uint32_t m_fid;
  uint64_t m_offset;
  uint32_t m_count;
  std::unique_ptr<uint8_t[]> m_data;

  public:
  TWrite(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond(void);
};

class TClunk : public TRequest {
  private:
  uint32_t m_fid;

  public:
  TClunk(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TRemove : public TRequest {
  private:
  uint32_t m_fid;

  public:
  TRemove(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TStat : public TRequest {
  private:
  uint32_t m_fid;

  std::unique_ptr<p9_stat_t> m_stat;

  public:
  TStat(uint16_t tag, uint8_t *body, size_t size);
  bool Execute(P9Core *core);
  RResponse *Respond();
};

class TWstat : public TRequest {
  private:
  uint32_t m_fid;
  std::unique_ptr<uint8_t[]> m_stat;

  public:
  TWstat(uint16_t tag, uint8_t *body, size_t size);
};

#endif
