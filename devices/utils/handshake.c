#include "handshake.h"
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "iostructs.h"

int DeviceWrite(int fd, char *str) {
  int err;
  int len = strlen(str);
  err = write(fd, str, len);
  if (err != len)
    return -1;
  return 0;
}

int DeviceHandshake(int fd, int *vcpu_fds, size_t *nvcpus) {
  int err,ret = 0;
  err = DeviceWrite(fd, (char *)"INIT");
  if (err != 0) {
    printf("Error on DeviceWrite\n");
  }

  struct init_response init = {0};

  if (read(fd, &init, sizeof(init)) < sizeof(init)) {
    fprintf(stderr, "Failed to read init response\n");
    return -1;
  }

  ret = strncmp((const char *)&init.magic, "TINI", 4);

  memcpy(vcpu_fds, init.fds, sizeof(init.fds));

  int i = 0;
  for (i=0;i<NR_MAX_VCPUS;i++) {
    if (!init.fds[i]) {
      break;
    }
  }
  *nvcpus = i;

  return ret;
}

int HandledRequest(int fd, int val) {
  int err;
  err = write(fd, &val, 4);
  if (!err)
    perror("Write handled req\n");
  return err;
}
