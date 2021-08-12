#pragma once
#include "utils/coms.h"

#define HOST_VGA_SIZE   0x20000
#define PIO_READ 0
#define PIO_WRITE 1

// Modes
#define VGA_MODE_TEXT 0x3
#define VGA_MODE_VIDEO 0x13

// Ports
// R/W
#define VGA_PORT_CHANGE_MODE 0x3b0

#define VGA_MEM_START 0xA0000
#define TEXT_MEM_START 0xB8000
#define TEXT_MEM_START_OFF 0x18000
#define VIDEO_MEM_START VGA_MEM_START

#define VGA_VIDEO_WIDTH 320
#define VGA_VIDEO_HEIGHT 200
#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25

// 80x25 <--- NOTE: this is "cells" graphics res is 720x400 w/ char size of 9x16
//#define TEXT_PLANE_SIZE_BYTES 80*25*sizeof(char_type)
#define TEXT_PLANE_SIZE_BYTES 4000
// 320x200
#define VIDEO_PLANE_SIZE_BYTES 64000

// MODE         cols x row       char size      graphics res      colors/mem model
//  3	VGA Text  	80×25	            9×16         	720×400	           16/CTEXT
// VGA TEXT MEM STARTS AT 0xB8000
enum vga_color {
  vga_black,
  vga_blue,
  vga_green,
  vga_cyan,
  vga_red,
  vga_purple,
  vga_brown,
  vga_gray
};

typedef union char_type {
  struct {
    uint32_t character:8;
    uint32_t attribute:8;
  } fields;
  uint16_t val;
} char_type;


union char_attr {
  struct {
    uint32_t forecolor:4;
    uint32_t backcolor:4;
  } fields;
  uint8_t val;
};

typedef struct ooows_vga_dev {
  uint32_t width;
  uint32_t height;
  uint8_t mode;
  uint8_t *mem;
  // 80x25
  uint8_t *text_plane;
  char *shm_text_name;
  pthread_mutex_t vga_lock;

  // 320x200
  // 0 --------------------- 320
  // |
  // |
  // |
  // 199
  uint8_t *vplane;
  char *shm_video_name;
  struct com_t *com;
} ooows_vga_dev;

int DestroyDevice(void);
void FrontendHandshake(void *vp);
void SendUpdate(struct com_t *com, char m);
