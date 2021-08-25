#pragma once

#include "ioooctls.h"
#include "vmexit.h"
#include "memory.h"
#include "vmcs.h"

typedef struct vm_t vm_t;

#define OOO_VMEXIT_HANDLED -1

typedef struct vcpu_t
{
	__u8 msr_bitmap[0x4000];
	struct __vmcs_t vmcs;
	__u64 vmcs_pa;
	ooo_state_t state;
	vm_t *vm;

	__u8 host_cpu_id;
	__u8 guest_cpu_id;
	__u8 launched;
	__u8 initialized;

	char padding[0x1000-((sizeof(ooo_state_t)+8+8+1+1+1+1)&0xfff)];
} __attribute__((packed)) vcpu_t;

typedef struct vm_t
{
	unsigned char memory[0x200000];
	EPTP eptp[512];
	EPT_PML4E pml4[512]; // one page of pml4
	EPT_PDPTE pdpt[512]; // one page of pdpte
	EPT_PDE pd[512]; // one page of PDEs
	// BUG: there are too few pagetables so faults above 1GB cause OOB accesses
	EPT_PTE pt[128][512]; // 1 gig of addressable memory

	vcpu_t vcpus[MAX_VCPUS];
	char padding[0x1000-((sizeof(vcpu_t)*MAX_VCPUS)&0xfff)];
} vm_t;

struct processor_init_arg
{
	vm_t *vm;
	int ret;
};

struct instruction_decoding
{
	unsigned char instruction_data[16];
	__u8 instruction_length;

	__u8 addr_reg1_offset;
	__u8 addr_reg1_size;
	__u8 addr_reg2_offset;
	__u8 addr_reg2_size;
	__u8 addr_reg2_scale;
	__u16 addr_offset;
	__u8 addr_segment;

	__u8 data_reg;
	__u32 data_imm;

	__u8 direction;
	__u8 operation;
};

// funcs
struct vcpu_t *alloc_vcpu(void);
vm_t *vm_alloc(void);
int vcpu_run(vcpu_t *);
int vcpu_destroy(struct vcpu_t *vcpu);
int vm_destroy(vm_t *vm);
void test_fn(void *fart);
void my_exit(void);
int vmexit_handle(struct vcpu_t *vcpu);
void eptp_destroy(EPTP *);
int ept_map(vm_t *vm, __u64, void *);
int ept_map_range(vm_t *vm, __u64, void *, __u64);
void ept_unmap(vm_t *vm, __u64);
void ept_unmap_range(vm_t *vm, __u64, __u64);
void *guest_to_host(vm_t *vm, __u64);
void vmx_on(void);
void vmx_off(void);


void write_to_guest(vm_t *, void *, void *, int);
