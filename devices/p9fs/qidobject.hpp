#ifndef QIDOBJECT_H_
#define QIDOBJECT_H_

#include "iostructs.h"
#include "trace.h"
#include <memory>
#include <string>
#include <atomic>

#define DMDIR 0x80000000

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint32_t version;
  uint64_t path;
} qid_t;

typedef struct {
  uint16_t size;
  uint16_t type;
  uint32_t dev;
  qid_t qid;
  uint32_t mode;
  uint32_t atime;
  uint32_t mtime;
  uint64_t length;
} p9_stat_t;

enum {
    P9_QTDIR = 0x80,
    P9_QTAPPEND = 0x40,
    P9_QTEXCL = 0x20,
    P9_QTMOUNT = 0x10,
    P9_QTAUTH = 0x08,
    P9_QTTMP = 0x04,
    P9_QTSYMLINK = 0x02,
    P9_QTLINK = 0x01,
    P9_QTFILE = 0x00,
};

enum {
      P9_OREAD,
      P9_OWRITE,
      P9_ORDWR,
      P9_OEXEC
};

class QidObject {
  private:
  uint8_t m_type;
  uint32_t m_version;
  uint64_t m_path;

  std::string m_fspath;
  std::string m_root;

  bool m_opened = false;
  uint8_t m_mode;
  int m_fd;

  std::atomic<uint32_t> m_refcnt{0};

  public:
  ~QidObject();
  QidObject(std::string path, std::string root);
  uint8_t Type() { return m_type; }
  uint64_t Path() { return m_path; }
  bool Opened() { return m_opened; }
  bool IsRoot() { return !m_fspath.compare(m_root); }
  void IncRef() { int cnt = ++m_refcnt; TRACE_PRINT("inc: %d", cnt); }
  void DecRef() {
    int cnt = --m_refcnt;
    TRACE_PRINT("dec: %d", cnt);
    if (!cnt) {
      delete this;
    }
  }
  void Qid(qid_t *q);
  bool Open(uint8_t mode);
  bool Close();
  std::unique_ptr<uint8_t[]> Read(uint64_t offset, uint32_t& count);
  uint32_t Write(uint64_t offset,  std::unique_ptr<uint8_t[]> data, uint32_t count);
  std::unique_ptr<p9_stat_t> Stat();
  bool Remove();
  QidObject *CreateChild(std::string child, uint32_t perm);
  QidObject *Traverse(std::string next);
};
#endif
