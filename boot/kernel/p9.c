#include <stdint.h>
#include <stddef.h>

#include "p9.h"
#include "kprint.h"
#include "malloc.h"

uint32_t fid = 1;
uint32_t tag = 0x100;

void p9_version(void *addr) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_version_msg_body_t) +\
    sizeof(p9_string_t) +\
    strlen("P92021");

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TVERSION;
  msg->tag = tag++;

  p9_version_msg_body_t *body = (p9_version_msg_body_t *)msg->body;
  body->msize = 0x10000;

  p9_string_t *version = (p9_string_t *)body->tail;
  version->size = strlen("P92021");
  memcpy(version->data, "P92021", strlen("P92021"));
}

void p9_auth(void *addr, char *u, char *a) {
  u = "pizza";
  a = "who";
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_auth_msg_body_t) +\
    sizeof(p9_string_t) +\
    sizeof(p9_string_t) +\
    strlen(u) +\
    strlen(a);

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TAUTH;
  msg->tag = tag++;

  p9_auth_msg_body_t *body = (p9_auth_msg_body_t *)msg->body;
  body->afid = fid++;

  p9_string_t *uname = (p9_string_t *)body->tail;
  uname->size = strlen(u);
  memcpy(uname->data, u, strlen(u));

  p9_string_t *aname = (p9_string_t *)(body->tail +\
                                       uname->size +\
                                       sizeof(p9_string_t));

  aname->size = strlen(a);
  memcpy(aname->data, a, strlen(a));
}

int p9_attach(void *addr, char *u, char *a) {
  // calculate the size of the attach message
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_attach_msg_body_t) +\
    sizeof(p9_string_t) +\
    sizeof(p9_string_t) +\
    strlen(u) +\
    strlen(a);

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TATTACH;
  msg->tag = tag++;

  p9_attach_msg_body_t *body = (p9_attach_msg_body_t *)msg->body;
  body->fid = fid++;
  body->afid = -1;

  p9_string_t *uname = (p9_string_t *)body->tail;
  uname->size = strlen(u);
  memcpy(uname->data, u, strlen(u));

  p9_string_t *aname = (p9_string_t *)(body->tail +     \
                                       uname->size +    \
                                       sizeof(p9_string_t));

  aname->size = strlen(a);
  memcpy(aname->data, a, strlen(a));

  return body->fid;
}

int p9_walk(void *addr, int fid, int newfid, size_t n, char **wnames) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_walk_msg_body_t);

  for(int i = 0; i < n; i++) {
    total += sizeof(p9_string_t);
    total += strlen(wnames[i]);
  }

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TWALK;
  msg->tag = tag++;

  p9_walk_msg_body_t *body = (p9_walk_msg_body_t *)msg->body;
  body->fid = fid;
  body->newfid = newfid;

  body->nwname = n;
  uint8_t *tail = body->tail;
  for(int i = 0; i < n; i++) {
    p9_string_t *wn = (p9_string_t *)tail;
    wn->size = strlen(wnames[i]);
    memcpy(wn->data, wnames[i], strlen(wnames[i]));
    tail += sizeof(p9_string_t) + wn->size;
  }

  return body->newfid;
}

int p9_create(void *addr, int fid, char *newf, uint32_t perms, uint8_t mode) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_create_msg_body_t) +\
    sizeof(p9_string_t) +\
    strlen(newf) +\
    sizeof(uint32_t) +\
    sizeof(uint8_t);

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TCREATE;
  msg->tag = tag++;

  p9_create_msg_body_t *body = (p9_create_msg_body_t *)msg->body;
  body->fid = fid;

  p9_string_t *tail = (p9_string_t *)body->tail;
  tail->size = strlen(newf);

  memcpy(tail->data, newf, tail->size);

  uint32_t *permsp = (uint32_t *)(((uint8_t *)tail) + sizeof(p9_string_t) + tail->size);
  *permsp = perms;

  uint8_t *modep = (uint8_t *)permsp + sizeof(uint32_t);
  *modep = mode;

  return 0;
}

int p9_write(void *addr, int fid, uint32_t offset, uint8_t *data, uint32_t count) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_write_msg_body_t) +\
    count;

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TWRITE;
  msg->tag = tag++;

  p9_write_msg_body_t *body = (p9_write_msg_body_t *)msg->body;
  body->fid = fid;
  body->offset = offset & 0xffffffff;
  body->count = count;

  memcpy(body->tail, data, count);

  return 0;
}

int p9_read(void *addr, int fid, uint32_t offset, uint32_t count)
{
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_read_msg_body_t);

  p9_msg_t *msg = (p9_msg_t *)addr;

  msg->size = total;
  msg->type = P9_TREAD;
  msg->tag = tag++;

  p9_read_msg_body_t *body = (p9_read_msg_body_t *)msg->body;
  body->fid = fid;
  body->offset = offset & 0xffffffff;
  body->count = count;

  return 0;
}

int p9_open(void *addr, int fid, uint8_t mode) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_open_msg_body_t);

  p9_msg_t *msg = (p9_msg_t *)addr;

  msg->size = total;
  msg->type = P9_TOPEN;
  msg->tag = tag++;

  p9_open_msg_body_t *body = (p9_open_msg_body_t *)msg->body;
  body->fid = fid;
  body->mode = mode;

  return 0;
}

int p9_clunk(void *addr, int fid) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_clunk_msg_body_t);

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TCLUNK;
  msg->tag = tag++;

  p9_clunk_msg_body_t *body = (p9_clunk_msg_body_t *)msg->body;
  body->fid = fid;

  return 0;
}

int p9_remove(void *addr, int fid) {
  size_t total = sizeof(p9_msg_t) +\
    sizeof(p9_remove_msg_body_t);

  p9_msg_t *msg = (p9_msg_t *)addr;
  msg->size = total;
  msg->type = P9_TREMOVE;
  msg->tag = tag++;

  p9_remove_msg_body_t *body = (p9_remove_msg_body_t *)msg->body;
  body->fid = fid;

  return 0;
}
