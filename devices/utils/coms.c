#include "coms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

void *TxHandler(void *vp) {
  struct com_t *com = (struct com_t *)vp;
  int client_fd;
  int fd = com->fd;
  struct sockaddr_un *addr = com->addr;
  bind(fd, (struct sockaddr *)addr, sizeof(struct sockaddr_un));
  listen(fd, 1);

  while (1) {
    client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
      perror("TxHandler accept failed\n");
      return NULL;
    }
    com->num_clients = 1;
    close(com->client_fd);
    com->client_fd = client_fd;
    //printf("Got a new connection: %d\n", client_fd);
    if (com->handshake) {
      com->handshake(vp);
      //printf("Called handshake\n");
    }
  }
}

struct com_t *InitCom(char *name, void (*handshake_cb)(void *)) {
  struct com_t *com = CreateCom(name);
  if (!com)
    return NULL;
  if (handshake_cb != NULL) {
    com->handshake = handshake_cb;
    //printf("Set handshake fn\n");
  }
  pthread_t tid;
  tid = pthread_create(&tid, NULL, TxHandler, (void *)com);
  com->handler_tid = tid;
  return com;
}

struct com_t * CreateCom(char *name) {
  struct com_t *com = NULL;
  struct sockaddr_un *addr = NULL;
  addr = calloc(1, sizeof(struct sockaddr_un));
  com = calloc(1, sizeof(struct com_t));
  if (!com || !addr) {
    printf("Failed to allocate memory for com or socket\n");
    return NULL;
  }
  int fd;
  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket failed");
    return NULL;
  }
  addr->sun_family = AF_UNIX;
  strncpy(addr->sun_path, name, sizeof(addr->sun_path)-1);

  size_t sz = strlen(name) + strlen("-cache") + 1;
  com->cachepath = calloc(1, sz);
  if (!com->cachepath) {
    return NULL;
  }

  snprintf(com->cachepath, sz, "%s-cache", name);

  com->name = name;
  com->fd = fd;
  com->addr = addr;
  com->client_fd = -1;
  return com;
}

void DestroyCom(struct com_t *com) {
  if (com->addr)
    free(com->addr);
  free(com);
  return;
}

void SetHandshake(struct com_t *com, void (*func)(void *)) {
  com->handshake = func;
  return;
}

int SendMessage(struct com_t *com, struct message_t *msg) {
  if (com->client_fd < 0) {
    printf("No client\n");
    return -1;
  }

  int fd = open(com->cachepath, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
  if (fd >= 0) {
    write(fd, msg->data, msg->len);
    close(fd);
  }

  // TODO: Chunk this
  // TODO LOCK
  //printf("Writing %c, len %d, to %d\n", msg->data[0], msg->len, com->client_fd);
  int nwrote = write(com->client_fd, msg->data, msg->len);
  if (nwrote != msg->len) {
    printf("Didn't send all data!\n");
    return -1;
  }

  return 0;
}

struct message_t * RecvMessage(struct com_t *com) {
}
