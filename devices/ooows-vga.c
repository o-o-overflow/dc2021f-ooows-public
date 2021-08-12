#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	//write
#include <string.h>	//strlen
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "ooows-vga.h"
#include "iostructs.h"
#include "utils/coms.h"
#include "utils/per-vm.h"
#include "utils/handshake.h"
#include "utils/threadpool.h"

#define DEBUG 0

struct ooows_vga_dev *gVGA = NULL;

void sig_handler(int signum){
  DestroyDevice();
  exit(0);
};

int InitDevice(void) {
  gVGA = calloc(1, sizeof(struct ooows_vga_dev));
  if (!gVGA)
    goto fail;

  // init lock
  pthread_mutex_init(&gVGA->vga_lock, NULL);
  char *shm_text_name = concatName("/vga-shm-text");
  if (!shm_text_name)
    goto fail;
  int shm_text_fd = createShm(shm_text_name, 0);
  if (shm_text_fd < 0)
    goto fail;
  gVGA->shm_text_name = shm_text_name;
  ftruncate(shm_text_fd, TEXT_PLANE_SIZE_BYTES);
  // make room for 80*25 char_types
  gVGA->text_plane = mmap(NULL,
                      TEXT_PLANE_SIZE_BYTES,
                      PROT_READ|PROT_WRITE,
                      MAP_SHARED,
                      shm_text_fd,
                      0);
  if (gVGA->text_plane == MAP_FAILED) {
    perror("Failed to map text_plane");
    goto fail;
  }

  char *shm_video_name = concatName("/vga-shm-video");
  if (!shm_video_name)
    goto fail;
  gVGA->shm_video_name = shm_video_name;
  int shm_video_fd = createShm(shm_video_name, 0);
  if (shm_video_fd < 0)
    goto fail;
  gVGA->shm_video_name = shm_video_name;
  ftruncate(shm_video_fd, VIDEO_PLANE_SIZE_BYTES);
  gVGA->vplane = mmap(NULL,
                      VIDEO_PLANE_SIZE_BYTES,
                      PROT_READ|PROT_WRITE,
                      MAP_SHARED,
                      shm_video_fd,
                      0);
  if (gVGA->vplane == MAP_FAILED) {
    perror("Failed to map vplane");
    goto fail;
  }

  gVGA->mem = calloc(1, HOST_VGA_SIZE);
  if (!gVGA->mem) {
    goto fail;
  }

  // start in video mode
  gVGA->mode = VGA_MODE_VIDEO;
  gVGA->width = VGA_VIDEO_WIDTH;
  gVGA->height = VGA_VIDEO_HEIGHT;

  char *vmm_store_dir = getenv("OOOWS_VM_STORE_DIR");
  if (!vmm_store_dir)
    vmm_store_dir = "/tmp/vms";
  char *vmname = getenv("OOOWS_VM_NAME");
  if (!vmname)
    goto fail;
  char *sock_name = "vga-notifs";
  // 2 for slashes, 1 for null term (not actually needed)
  uint32_t space_needed = strlen(vmm_store_dir) + strlen(vmname) + strlen(sock_name) + 3;
  char *path = calloc(1, space_needed);
  if (!path)
    goto fail;
  snprintf(path, space_needed, "%s/%s/%s", vmm_store_dir, vmname, sock_name);
  unlink(path);
  gVGA->com = InitCom(path, &FrontendHandshake);
  if (!gVGA->com)
    goto fail;

  return 0;
fail:
  DestroyDevice();
  return -1;
}

int DestroyDevice(void) {
  if (!gVGA)
    return 0;

  if (gVGA->mem) {
    free(gVGA->mem);
    gVGA->mem = 0;
  }

  if (gVGA->vplane) {
    munmap(gVGA->vplane, VIDEO_PLANE_SIZE_BYTES);
    gVGA->vplane = 0;
  }

  if (gVGA->text_plane) {
    munmap(gVGA->text_plane, TEXT_PLANE_SIZE_BYTES);
    gVGA->text_plane = 0;
  }

  if (gVGA->shm_text_name) {
    shm_unlink(gVGA->shm_text_name);
    free(gVGA->shm_text_name);
    gVGA->shm_text_name = 0;
  }

  if (gVGA->shm_video_name) {
    shm_unlink(gVGA->shm_video_name);
    free(gVGA->shm_video_name);
    gVGA->shm_video_name = 0;
  }

  if (gVGA->com) {
    if (gVGA->com->name) {
      unlink(gVGA->com->name);
      free(gVGA->com->name);
    }
    DestroyCom(gVGA->com);
    gVGA->com = 0;
  }

  if (gVGA) {
    free(gVGA);
    gVGA = 0;
  }
  return 0;
}

void SendUpdate(struct com_t *com, char m) {
  struct message_t *msg = calloc(1, sizeof(struct message_t));
  msg->data = calloc(1, 1);
  msg->data[0] = m;
  msg->len = 1;
  SendMessage(gVGA->com, msg);
  free(msg->data);
  free(msg);
}

// let the web interface know what mode we're in
void FrontendHandshake(void *vp) {
  struct com_t *com = (struct com_t *)vp;
  // TODO: Lock??
  char m;
  if (gVGA->mode == 0x13)
    m = 'v';
  else if (gVGA->mode == 0x3)
    m = 't';
  // tell web clients about updated text mem
  SendUpdate(com, m);
}

int VgaChangeMode(struct ioport_request *pio) {
  // check correct direction
  if (pio->direction == PIO_READ) {
    return gVGA->mode;
  }

  uint8_t new_mode = pio->data;
  //printf("Guest wants to change mode to: 0x%x\n", new_mode);
  uint64_t start_addr;
  // change from text --> video
  switch (new_mode) {
    case VGA_MODE_VIDEO:
      gVGA->mode = VGA_MODE_VIDEO;
      gVGA->width = VGA_VIDEO_WIDTH;
      gVGA->height = VGA_VIDEO_HEIGHT;
      // copy in relevant memory
      memcpy(gVGA->vplane, gVGA->mem, VIDEO_PLANE_SIZE_BYTES);
      break;
    case VGA_MODE_TEXT:
      gVGA->mode = VGA_MODE_TEXT;
      gVGA->width = VGA_TEXT_WIDTH;
      gVGA->height = VGA_TEXT_HEIGHT;
      // copy in relevant memory
      start_addr = (uint64_t)gVGA->mem + TEXT_MEM_START_OFF;
      memcpy(gVGA->text_plane, (uint8_t *)start_addr, TEXT_PLANE_SIZE_BYTES);
      break;
    default:
      //printf("Bad mode selected\n");
      break;
  }

  return 0;
}

int VgaHandlePio(int fd, struct ioport_request *pio) {
  int ret = 0;

  switch(pio->port) {
    case VGA_PORT_CHANGE_MODE:
      //printf("Request to change mode\n");
      ret = VgaChangeMode(pio);
      break;
    default:
      fprintf(stderr, "Unknown port\n");
  }

end:
  HandledRequest(fd, ret);
  return 0;
}

int VgaTextWrite(struct mmio_request *mmio) {
  uint32_t index = mmio->phys_addr - TEXT_MEM_START;
  int ret = 0;

  // there's 25 rows of 80.
  // index % 80 = x       index // 80 = y
  // TODO: Check bounds
  if (index < ((gVGA->width * gVGA->height) - mmio->len)) {
    memcpy(&(gVGA->text_plane[index]), &mmio->data, mmio->len);
    //printf("copied %d bytes to the text plane at index %d\n", mmio->len, index);
  }
  else {
    //printf("Text write wasn't in bounds\n");
    return -1;
  }

  // tell web clients about updated text mem
  if (gVGA->com->num_clients > 0) {
    SendUpdate(gVGA->com, 't');
  }
  return ret;
}

int VgaVideoWrite(struct mmio_request *mmio) {
  int ret = 0;
  uint32_t index = mmio->phys_addr - VIDEO_MEM_START;
  // TODO: Check bounds
  if (index < ((gVGA->width * gVGA->height) - mmio->len)) {
    memcpy(&gVGA->vplane[index], (uint8_t *)&mmio->data, mmio->len);
  }
  else {
    //printf("Write out of current vram bounds\n");
    return -1;
  }

  // tell web clients about updated video mem
  if (gVGA->com->num_clients > 0) {
    SendUpdate(gVGA->com, 'v');
  }
  return ret;
}

int MirrorWrite(struct mmio_request *mmio) {
  uint32_t offset = mmio->phys_addr - VGA_MEM_START;
  if (DEBUG) {
    if (offset == 0) {
      printf("Mirroring write of 0x%lx at offset: 0x%x  of len: %d\n", mmio->data, offset, mmio->len);
    }
  }
  if (offset < ( HOST_VGA_SIZE - mmio->len)) {
    memcpy(&gVGA->mem[offset], (uint8_t *)&mmio->data, mmio->len);
  }
  return 0;
}

int MirrorRead(struct mmio_request *mmio) {
  uint32_t offset = mmio->phys_addr - VGA_MEM_START;
  //printf("Mirroring read of 0x%lx at offset: 0x%x  of len: %d\n", mmio->data, offset, mmio->len);
  if (offset < ( HOST_VGA_SIZE - mmio->len)) {
    memcpy((uint8_t *)&mmio->data, &gVGA->mem[offset], mmio->len);
  }
  //printf("read: 0x%lx\n", mmio->data);
  return 0;
}

int VgaHandleMmio(int fd, struct mmio_request *mmio) {
  int ret = 0;

  // if it's a read, just return the value from our mirrored mem
  if (!mmio->is_write) {
    MirrorRead(mmio);
    HandledRequest(fd, mmio->data);
    return 0;
  }

  // if it's a write, first mirror the write and then determine what kind of write we're dealing with
  MirrorWrite(mmio);

  switch (gVGA->mode) {
    case VGA_MODE_TEXT:
      ret = VgaTextWrite(mmio);
      break;
    case VGA_MODE_VIDEO:
      ret = VgaVideoWrite(mmio);
      break;
    default:
      printf("Unknown/Bad mode!\n");
      ret = -1;
  }

  HandledRequest(fd, 0);
  return ret;
}

void *VgaHandleIO(void *arg) {
  int err = 0;
  int fd = *(int *)arg;

  struct io_request io = {0};
  while (read(fd, &io, sizeof(io)) == sizeof(io)) {
    pthread_mutex_lock(&gVGA->vga_lock);
    switch(io.type) {
    case IOTYPE_PIO:
      err = VgaHandlePio(fd, &io.ioport);
      break;
    case IOTYPE_MMIO:
      err = VgaHandleMmio(fd, &io.mmio);
      break;
    default:
      fprintf(stderr, "Unknown IO type encountered: %d\n", io.type);
    }
    pthread_mutex_unlock(&gVGA->vga_lock);

    if (err == -2) return (void *)-1;
  }

  return (void *)0;
}

int main(void) {
  int err, ret;
  size_t nvcpus = 0;
  int vcpu_fds[NR_MAX_VCPUS] = {0};

  signal(SIGTERM, sig_handler);
  ret = DeviceHandshake(CHILD_DEVICE_CHANNEL_FD, vcpu_fds, &nvcpus);
  if (ret != 0) {
    //printf("Handshake failed\n");
    return -1;
  }

  err = InitDevice();
  if (err) {
    //printf("Device init failed\n");
    return -1;
  }

  int i = 0;
  pthread_t workers[NR_MAX_VCPUS] = {0};
  for(i=0;i<nvcpus;i++) {
    pthread_create(&workers[i], NULL, VgaHandleIO, &vcpu_fds[i]);
  }

  for(i=0;i<nvcpus;i++) {
    pthread_join(workers[i], NULL);
  }

  DestroyDevice();
  return err;
}
