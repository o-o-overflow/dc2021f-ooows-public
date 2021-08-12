#include "qidobject.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "trace.h"

QidObject::~QidObject() {
  TRACE_PRINT("Destroying %lx", m_path);
}

QidObject::QidObject(std::string path, std::string root)
  : m_fspath(path), m_version(0), m_root(root) {
  struct stat st;
  const char *cpath = m_fspath.c_str();

  TRACE_PRINT("Trying to stat %s\n", cpath);
  if (stat(cpath, &st) < 0) {
    throw std::exception();
  }

  TRACE_PRINT("Mode %d\n", st.st_mode);
  if (S_ISREG(st.st_mode)) {
    m_type = P9_QTFILE;
  } else if (S_ISDIR(st.st_mode)) {
    m_type = P9_QTDIR;
  } else if (S_ISLNK(st.st_mode)) {
    m_type = P9_QTSYMLINK;
  }

  int c;
  m_path = 5381;
  while (c = *cpath++) { m_path = ((m_path << 5) + m_path) + c; }
}

void QidObject::Qid(qid_t *q) {
  q->type = m_type;
  q->version = m_version;
  q->path = m_path;
}

bool QidObject::Open(uint8_t mode) {
  int flags = -1;

  if (m_opened) {
    TRACE_PRINT("Attempted to reopen %lx\n", m_path);
    return false;
  }

  if(m_type == P9_QTDIR) {
    m_opened = true;
    m_mode = mode;
    return true;
  }

  switch(mode) {
  case P9_OREAD:
    flags = O_RDONLY;
    break;
  case P9_OWRITE:
    flags = O_WRONLY;
    break;
  case P9_ORDWR:
    flags = O_RDWR;
    break;
  }

  if (flags < 0) {
    return false;
  }

  m_fd = open(m_fspath.c_str(), flags);
  if (m_fd < 0) {
    return false;
  }

  m_opened = true;
  m_mode = mode;

  TRACE_PRINT("Opened %s with %x at %d", m_fspath.c_str(), flags, m_fd);
  return true;
}

std::unique_ptr<uint8_t[]> QidObject::Read(uint64_t offset, uint32_t& count) {
  if (!m_opened) {
    return NULL;
  }

  if (lseek(m_fd, offset, SEEK_SET) < 0) {
    TRACE_PRINT("Failed to seek to offset %lx", offset);
    return NULL;
  }

  uint8_t *raw = new uint8_t[count];

  ssize_t new_count = read(m_fd, raw, count);
  if (new_count < 0) {
    delete[] raw;
    return NULL;
  }

  std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(new_count);
  memcpy(data.get(), raw, new_count);
  delete[] raw;

  count = new_count;

  return data;
}

uint32_t QidObject::Write(uint64_t offset,
                          std::unique_ptr<uint8_t[]> data,
                          uint32_t count) {
  if (!m_opened) {
    return false;
  }

  if (lseek(m_fd, offset, SEEK_SET)) {
    TRACE_PRINT("Failed to seek to offset %lx\n", offset);
  }

  ssize_t ret = write(m_fd, data.get(), count);
  if (ret < 0) {
    return 0;
  }
  return ret;
}

std::unique_ptr<p9_stat_t> QidObject::Stat() {
  struct stat st;

  if (stat(m_fspath.c_str(), &st) < 0) {
    return NULL;
  }

  std::unique_ptr<p9_stat_t> p9st = std::make_unique<p9_stat_t>();

  p9st->size = sizeof(p9_stat_t);
  Qid(&p9st->qid);
  p9st->length = st.st_size;

  return p9st;
}

bool QidObject::Close() {
  if (!m_opened) {
    return false;
  }
  close(m_fd);
  m_opened = false;
  return true;
}

bool QidObject::Remove() {
  if (unlink(m_fspath.c_str())) {
    return false;
  }

  return true;
}

QidObject *QidObject::CreateChild(std::string child, uint32_t perm) {
  std::string newpath(m_fspath);

  newpath.append("/");
  newpath.append(child);

  if (perm & DMDIR) {
    TRACE_PRINT("Create new directory %s with %o", newpath.c_str(), perm & 0777);
    if (mkdir(newpath.c_str(), perm & 0777)) {
      return NULL;
    }
  } else {
    TRACE_PRINT("Creating new file %s with %o", newpath.c_str(), perm & 0777);
    int fd = open(newpath.c_str(), O_CREAT|O_EXCL, perm & 0777);
    if (fd < 0) {
      TRACE_PRINT("Failed to create new file %s", newpath.c_str());
      return NULL;
    }
    close(fd);
  }

  return Traverse(child);
}

QidObject *QidObject::Traverse(std::string next) {
  std::string newpath(m_fspath);

  newpath.append("/");
  newpath.append(next);

  QidObject *qobj = NULL;

  if (IsRoot() && !next.compare("..")) {
    newpath = m_fspath;
  }

  char resolved[PATH_MAX];
  if (!realpath(newpath.c_str(), resolved)) {
    TRACE_PRINT("Traverse realpath failed on %s\n", newpath.c_str());
    return NULL;
  }

  newpath = std::string(resolved);

  try {
    return new QidObject(newpath, m_root);
  } catch(std::exception e) {
    return NULL;
  }
}
