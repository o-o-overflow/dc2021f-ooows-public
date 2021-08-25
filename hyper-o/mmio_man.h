#pragma once
#include "inc/ioooctls.h"
#include <stdint.h>
#include <capstone/capstone.h>
#include "vmm/inc/iostructs.h"

//enum {
//      IOTYPE_PIO,
//      IOTYPE_MMIO,        
//};

//struct ioport_request {
//  uint32_t port:16;
//  uint32_t direction:8;
//  uint32_t size:8;
//  uint32_t data;
//  uint32_t count;
//};
//
//struct mmio_request {                            
//  uint64_t phys_addr;
//  //uint8_t data[8];
//  uint64_t data;
//  uint32_t len;
//  uint8_t is_write;
//} __attribute__((__packed__));
//
//struct io_request {
//  uint8_t type;
//  union {
//    struct ioport_request ioport;
//    struct mmio_request mmio;
//  };
//};


int handle_mmio(ooo_state_t *state, void *guest_mem);
int get_capstone_mode(int ooo_mode);
int apply_mmio(ooo_state_t *state, void *bytes, int capstone_mode);
int get_reg_offset(x86_reg cs_reg);
int debug_step(ooo_state_t *state, void *guest_mem);
