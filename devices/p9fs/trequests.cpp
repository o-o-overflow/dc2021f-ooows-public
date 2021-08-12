#include <string.h>
#include <string>

#include "trequests.hpp"
#include "trace.h"

#define READ(d, l) do {                         \
    if (size < l) throw std::exception();       \
    memcpy(d, body + offset, l);                \
    size -= l;                                  \
    offset += l;                                \
  } while (0)

#define READVAL(d) READ(&d, sizeof(d))

#define READSTR(str) do {                         \
    uint16_t __l;                                 \
    READVAL(__l);                                 \
    char *__s = new char[__l];                    \
    READ(__s, __l);                               \
    str = __s;                                    \
    delete[] __s;                                 \
  } while (0)

static bool LegalName(std::string s) {

  char l[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
    "abcdefghijklmnopqrstuvwxyvz"\
    "1234567890"\
    "_-.";

  std::string legal(l);
  for(auto it = s.begin(); it != s.end(); it++) {
    TRACE_PRINT("l: %c\n", *it);
    if (legal.find(*it) == std::string::npos) {
      return false;
    }
  }
  return true;
}

TRequest::TRequest(uint8_t type, uint16_t tag) : m_type(type), m_tag(tag) { }

void TRequest::SetError(std::string err) {
  m_errored = true;
  m_error = err;
}

RResponse *TRequest::Process(P9Core *core) {

  if (core->HasRequest(m_tag)) {
    core->CancelRequest(m_tag);
  } else {
    core->RegisterRequest(this);
  }

  if (Valid()) {
    m_completed = Execute(core);
  } else {
    SetError("Invalid TMesg");
  }

  RResponse *response = NULL;
  if (m_errored) {
    TRACE_PRINT("[%x] errored %s", m_tag, m_error.c_str());
    response = GenerateError();
  } else {
    response = Respond();
  }

  core->UnregisterRequest(this);

  return response;
}

RResponse *TRequest::GenerateError() {
  return new RError(m_tag, m_error);
}

TVersion::TVersion(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TVERSION, tag) {
  size_t offset = 0;

  READVAL(m_msize);

  READSTR(m_version);
}

bool TVersion::Execute(P9Core *core) {
  // TODO fix this
  // may need to negotiate this before processing further messages
  if (m_version.compare("P92021")) {
    SetError("Invalid version string");
    return false;
  }

  return true;
}

void TVersion::show(void) {
  TRACE_PRINT("[%x] Version: %s", m_tag, m_version.c_str());
}

RResponse *TVersion::Respond() {
  return new RVersion(m_tag, m_msize, m_version);
}

TAuth::TAuth(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TAUTH, tag) {
  size_t offset = 0;

  READVAL(m_afid);

  READSTR(m_uname);
  READSTR(m_aname);
}

void TAuth::show(void) {
  TRACE_PRINT("[%x] Afid: %d Uname: %s Aname: %s",
              m_tag, m_afid, m_uname.c_str(), m_aname.c_str());
}

bool TAuth::Execute(P9Core *core) {
  if (!core->Authenticated() && core->MustAuth()) {

  } else {
    SetError("Authentication not required");
  }
  return true;
}

RResponse *TAuth::Respond() {
  return new RAuth(m_tag, m_aqid);
}

TFlush::TFlush(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TFLUSH, tag) {
  size_t offset = 0;

  READVAL(m_oldtag);
}

bool TFlush::Execute(P9Core *core) {
  return true;
}

RResponse *TFlush::Respond() {
  return new RFlush(m_tag);
}

TAttach::TAttach(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TATTACH, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READVAL(m_afid);

  READSTR(m_uname);
  READSTR(m_aname);
}

bool TAttach::Execute(P9Core *core) {
  if (!core->Authenticated() && core->MustAuth()) {
    // check afid
  } else {
    if (m_afid != -1) {
      SetError("Incorrect Afid");
      return false;
    }

    if (!core->Serving(m_aname)) {
      SetError("No such aname");
      return false;
    }

    QidObject *qobj = core->Attach(m_aname);

    if (!core->BindFid(m_fid, qobj)) {
      SetError("Fid already claimed");
      delete qobj;
      return false;
    }

    qobj->Qid(&m_qid);
  }

  return true;
}

RResponse *TAttach::Respond() {
  return new RAttach(m_tag, m_qid);
}

TWalk::TWalk(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TWALK, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READVAL(m_newfid);

  // TODO does this copy correctly?
  uint16_t nwname;
  READVAL(nwname);
  for (int i = 0; i < nwname; i++) {
    std::string tmp;
    READSTR(tmp);
    m_wnames.push_back(tmp);
  }
}

bool TWalk::Valid() {
  for (auto it = m_wnames.begin(); it != m_wnames.end(); it++) {
    if (!LegalName(*it)) {
      return false;
    }
  }
  return true;
}

bool TWalk::Execute(P9Core *core) {
  QidObject *base = core->GetFid(m_fid);
  if (!base) {
    SetError("No such fid");
    return false;
  }

  if (base->Type() != P9_QTDIR) {
    SetError("Fid is not a directory");
    return false;
  }

  QidObject *next = base;
  m_qids.push_back(base);
  for (auto it = m_wnames.begin(); it != m_wnames.end(); it++) {
    next = next->Traverse(*it);
    if (next) {
      m_qids.push_back(next);
    } else {
      TRACE_PRINT("INVALID PATH %s", it->c_str());
      SetError("Invalid path");
      return false;
    }
  }

  // TODO fix clone case, home grown refcounting?
  if (m_fid == m_newfid) {
    core->RemoveFid(m_fid);
  }

  // now associate the new fid with the back
  QidObject *newQid = m_qids.back();
  TRACE_PRINT("[%x] Binding %lx to %d", m_tag, newQid->Path(), m_newfid);
  if (!core->BindFid(m_newfid, newQid)) {
    SetError("Newfid already bound");
    return false;
  }

  return true;
}

void TWalk::show(void) {
  TRACE_PRINT("[%x] Fid: %u Newfid: %u", m_tag, m_fid, m_newfid);

  for(auto it = m_wnames.begin(); it != m_wnames.end(); it++) {
    TRACE_PRINT("\t%s", it->c_str());
  }
}

RResponse *TWalk::Respond() {
  // TODO bug here, we'll pass our list by reference
  // if they can cancel this request while the list is going out
  // they can get a UAF in this
  return new RWalk(m_tag, m_qids);
}

TOpen::TOpen(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TOPEN, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READVAL(m_mode);
}

bool TOpen::Execute(P9Core *core) {
  QidObject *qid = core->GetFid(m_fid);
  if (!qid) {
    SetError("No such fid");
    return false;
  }

  TRACE_PRINT("[%x] Found %d at Qid %lx", m_tag, m_fid, qid->Path());
  if (qid->Type() != P9_QTFILE) {
    SetError("Can only read from files");
    return false;
  }

  if (!qid->Open(m_mode)) {
    SetError("Failed to open");
    return false;
  }

  qid->Qid(&m_qid);

  return true;
}

void TOpen::show(void) {
  TRACE_PRINT("[%x] Open of %d with mode %x", m_tag, m_fid, m_mode);
}

RResponse *TOpen::Respond() {
  return new ROpen(m_tag, m_qid);
}

TCreate::TCreate(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TCREATE, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READSTR(m_name);
  READVAL(m_perm);
  READVAL(m_mode);
}

bool TCreate::Valid() {
  return LegalName(m_name);
}

bool TCreate::Execute(P9Core *core) {
  // fid initially points to the parent directory
  QidObject *base = core->GetFid(m_fid);
  if (!base) {
    SetError("No such fid");
    return false;
  }

  // requested parent must be a directory
  if (base->Type() != P9_QTDIR) {
    TRACE_PRINT("Fid %d [%lx] is not directory", m_fid, base->Path());
    SetError("Fid is not a directory");
    return false;
  }

  // create the child
  QidObject *child = base->CreateChild(m_name, m_perm);
  if (!child) {
    SetError("Failed to create");
    return false;
  }

  // now open it up as the spec says
  if (!child->Open(m_mode)) {
    SetError("Failed to open");
    return false;
  }

  // finally we must replace the old fid
  core->RemoveFid(m_fid);
  if (!core->BindFid(m_fid, child)) {
    SetError("Failed to bind to fid");
    return false;
  }

  child->Qid(&m_qid);
  return true;
}

void TCreate::show(void) {
  TRACE_PRINT("[%x] Fid: %d Create %s %o %x", m_tag, m_fid, m_name.c_str(), m_perm, m_mode);
}

RResponse *TCreate::Respond() {
  return new RCreate(m_tag, m_qid);
}

TRead::TRead(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TREAD, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READVAL(m_offset);
  READVAL(m_count);
}

bool TRead::Execute(P9Core *core) {
  QidObject *obj = core->GetFid(m_fid);
  if (!obj) {
    SetError("No such fid");
    return false;
  }

  if (obj->Type() != P9_QTFILE) {
    SetError("Can only read from files");
    return false;
  }

  if (!obj->Opened()) {
    SetError("Fid must be open to read");
    return false;
  }

  // TODO test count against the core's MSize
  // TODO simple bug here by setting msize
  if (m_count > 0x1000) {
    SetError("Count too large");
    return false;
  }

  m_data = obj->Read(m_offset, m_count);
  if (!m_data) {
    SetError("Read failed");
    return false;
  }

  return true;
}

void TRead::show() {
  TRACE_PRINT("[%x] Read of %d at %lx %d\n", m_tag, m_fid, m_offset, m_count);
}

RResponse *TRead::Respond(void) {
  TRACE_PRINT("[%x] Read complete with %d bytes %s\n", m_tag, m_count, m_data.get());
  return new RRead(m_tag, m_count, std::move(m_data));
}

TWrite::TWrite(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TWRITE, tag) {
  size_t offset = 0;

  READVAL(m_fid);
  READVAL(m_offset);
  READVAL(m_count);

  m_data = std::make_unique<uint8_t[]>(m_count);
  READ(m_data.get(), m_count);
}

bool TWrite::Execute(P9Core *core) {
  QidObject *obj = core->GetFid(m_fid);
  if (!obj) {
    SetError("No such fid");
    return false;
  }

  TRACE_PRINT("Preparing to write to %d\n", m_fid);

  if (obj->Type() != P9_QTFILE) {
    SetError("Can only write to files");
    return false;
  }

  if (!obj->Opened()) {
    SetError("Fid must be open to write");
    return false;
  }

  // update count with how many bytes were written
  m_count = obj->Write(m_offset, std::move(m_data), m_count);
  if (!m_count) {
    SetError("Failed to write to fid");
    return false;
  }

  return true;
}

RResponse *TWrite::Respond() {
  return new RWrite(m_tag, m_count);
}

TClunk::TClunk(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TCLUNK, tag) {
  size_t offset = 0;

  READVAL(m_fid);
}

bool TClunk::Execute(P9Core *core) {
  QidObject *obj = core->GetFid(m_fid);
  if (!obj) {
    SetError("No such fid");
    return false;
  }

  TRACE_PRINT("Clunking %d", m_fid);

  bool ret = obj->Close();
  TRACE_PRINT("Qid %lx closed", obj->Path());

  // Remember the spec says even if this call fails
  // we must invalidate the fid
  core->RemoveFid(m_fid);
  TRACE_PRINT("Fid %d removed", m_fid);

  if (!ret) {
    SetError("Failed to clunk fid");
    return ret;
  }

  TRACE_PRINT("Clunked %d", m_fid);

  return ret;
}

RResponse *TClunk::Respond() {
  return new RClunk(m_tag);
}

TRemove::TRemove(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TREMOVE, tag) {
  size_t offset = 0;

  READVAL(m_fid);
}

bool TRemove::Execute(P9Core *core) {
  QidObject *obj = core->GetFid(m_fid);
  if (!obj) {
    SetError("No such fid");
    return false;
  }

  if (obj->Opened()) {
    if (!obj->Close()) {
      SetError("Failed to close fid");
      return false;
    }
  }

  if (!obj->Remove()) {
    SetError("Failed to remove fid");
    return false;
  }

  core->RemoveFid(m_fid);

  return true;
}

RResponse *TRemove::Respond() {
  return new RRemove(m_tag);
}

TStat::TStat(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TSTAT, tag) {
  size_t offset = 0;

  READVAL(m_fid);
}

bool TStat::Execute(P9Core *core) {
  QidObject *obj = core->GetFid(m_fid);
  if (!obj) {
    SetError("No such fid");
  }

  m_stat = obj->Stat();
  if (!m_stat) {
    SetError("Failed to stat");
    return false;
  }

  return true;
}

RResponse *TStat::Respond() {
  return new RStat(m_tag, std::move(m_stat));
}

TWstat::TWstat(uint16_t tag, uint8_t *body, size_t size) : TRequest(P9_TWSTAT, tag) {
  size_t offset = 0;

  READVAL(m_fid);

  uint16_t nstat;
  READVAL(nstat);

  m_stat = std::make_unique<uint8_t[]>(nstat);
  READ(m_stat.get(), nstat);
}
