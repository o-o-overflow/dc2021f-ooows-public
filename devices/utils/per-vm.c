#include "per-vm.h"
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>

int createShm(char *name, bool is_rdonly) {
  // try unlinking first just in case there was a previous messy exit
  int ret = shm_unlink(name);
  //printf("unlink ret: %d\n", ret);
  uint32_t mode = is_rdonly ? O_RDONLY : O_RDWR;
  int fd = shm_open(name, O_CREAT|O_TRUNC|mode, S_IRWXU);
  if (fd < 0) {
    perror("shm_open in createShm");
    return -1;
  }
  return fd;
}

char *concatName(char *base) {
  int ret;
  char *vmname = NULL;
  vmname = getenv("OOOWS_VM_NAME");
  if (!vmname)
    return NULL;
  int vmname_len = strlen(vmname);
  // 1 for dash, 1 for null terminator (yes I know snprintf accounts for this)
  int space_needed = strlen(base) + vmname_len + 2;
  char *new_str = calloc(1, space_needed);
  if (!new_str)
    return NULL;
  const char *format = "%s-%s";
  ret = snprintf(new_str, space_needed, format, base, vmname);
  if (ret < 0) {
    free(new_str);
    return NULL;
  }
  return new_str;
}
