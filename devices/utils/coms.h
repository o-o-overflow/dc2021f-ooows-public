#pragma once
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

struct com_t {
  char *name;
  char *cachepath;
  struct sockaddr_un *addr;
  int fd;
  int client_fd;
  uint32_t num_clients;
  bool connected;
  pthread_t handler_tid;
  void (*handshake)(void *);
};

struct message_t {
  uint32_t len;
  uint8_t *data;
};

struct com_t *InitCom(char *name, void (*handshake_cb)(void *));
struct com_t * CreateCom(char *name);
int SendMessage(struct com_t *com, struct message_t *msg);
struct message_t * RecvMessage(struct com_t *com);
void DestroyCom(struct com_t *com);
void SetHandshake(struct com_t *com, void (*func)(void *));
