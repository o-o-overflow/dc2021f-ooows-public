#include "trequests.hpp"
#include "qidobject.hpp"
#include "p9core.hpp"
#include "trace.h"

P9Core::P9Core(bool auth_required,
               std::string sharename,
               std::string mountpoint)
  : m_authed(false),
    m_auth_required(auth_required),
    m_sharename(sharename),
    m_mountpoint(mountpoint) { }

bool P9Core::Serving(std::string& point) {
  return !m_sharename.compare(point);
}

bool P9Core::BindFid(uint32_t fid, QidObject *qobj) {
  if (m_fid_map.count(fid)) {
    return false;
  }
  qobj->IncRef();
  m_fid_map[fid] = qobj;
  return true;
}

void P9Core::RemoveFid(uint32_t fid) {
  QidObject *qobj = m_fid_map[fid];

  m_fid_map.erase(fid);
  qobj->DecRef();
  TRACE_PRINT("Erased %d from map", fid);
}

QidObject * P9Core::GetFid(uint32_t fid) {
  if (m_fid_map.count(fid)) {
    return m_fid_map[fid];
  }
  return NULL;
}

QidObject * P9Core::Attach(std::string& point) {
  // ignore point for now, could map to a list of mountpoints
  return new QidObject(m_mountpoint, m_mountpoint);
}

bool P9Core::HasRequest(uint32_t tag) {
  return !!m_request_map.count(tag);
}

void P9Core::CancelRequest(uint32_t tag) {
  m_request_map[tag]->Cancel();
}

void P9Core::RegisterRequest(TRequest *trequest) {
  m_request_map[trequest->Tag()] = trequest;
}

void P9Core::UnregisterRequest(TRequest *trequest) {
  m_request_map.erase(trequest->Tag());
}
