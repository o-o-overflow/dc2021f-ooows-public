#ifndef HANDSHAKE_H_
#define HANDSHAKE_H_
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int DeviceHandshake(int fd, int *vcpu_fds, size_t *nvcpus);
int HandledRequest(int fd, int val);

#ifdef __cplusplus
}
#endif

#endif
