#ifndef P9_H_
#define P9_H_
#include <stdint.h>

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
  uint16_t size;
  char data[0];
} p9_string_t;

typedef struct __attribute__((packed)) {
  uint32_t msize;
  uint8_t tail[0];
} p9_version_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t afid;
  uint8_t tail[0];
} p9_auth_msg_body_t;

typedef struct  __attribute__((packed)) {
  uint32_t fid;
  uint32_t afid;
  uint8_t tail[0];
} p9_attach_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
  uint32_t newfid;
  uint16_t nwname;
  uint8_t tail[0];
} p9_walk_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
  uint8_t tail[0];
} p9_create_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
  uint64_t offset;
  uint32_t count;
  uint8_t tail[0];
} p9_write_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
  uint64_t offset;
  uint32_t count;
} p9_read_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
  uint8_t mode;
} p9_open_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
} p9_clunk_msg_body_t;

typedef struct __attribute__((packed)) {
  uint32_t fid;
} p9_remove_msg_body_t;

typedef struct  __attribute__((packed)) {
  size_t size;
  uint8_t type;
  uint16_t tag;
  uint8_t body[0];
} p9_msg_t;

void p9_version(void  *);
void p9_auth(void *, char *, char *);
int p9_attach(void *, char *, char *);
int p9_walk(void *, int, int, size_t n, char **);
int p9_create(void *, int, char *, uint32_t perms, uint8_t mode);
int p9_write(void *, int, uint32_t, uint8_t *, uint32_t count);
int p9_read(void *, int, uint32_t, uint32_t);
int p9_open(void *, int, uint8_t);
int p9_clunk(void *, int);
int p9_remove(void *, int);
#endif
