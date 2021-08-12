#ifndef P9CORE_H_
#define P9CORE_H_

#include "qidobject.hpp"
#include <map>

class TRequest;

class P9Core {
  private:
  bool m_authed;
  bool m_auth_required;
  std::string m_sharename;
  std::string m_mountpoint;
  std::map<uint32_t, QidObject *> m_fid_map;
  std::map<uint32_t, TRequest *> m_request_map;

  public:
  bool Authenticated() { return m_authed; }
  bool MustAuth() { return m_auth_required; }
  int Auth(char *uname);

  bool BindFid(uint32_t fid, QidObject *obj);
  void RemoveFid(uint32_t fid);
  QidObject *GetFid(uint32_t fid);
  QidObject *Attach(std::string& point);
  bool Serving(std::string& point);

  bool HasRequest(uint32_t tag);
  void CancelRequest(uint32_t tag);
  void RegisterRequest(TRequest *trequest);
  void UnregisterRequest(TRequest *trequest);

  P9Core(bool m_auth_required, std::string sharename, std::string m_mountpoint);
};
#endif
