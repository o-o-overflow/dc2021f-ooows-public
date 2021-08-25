#include "../vmm/inc/vmm.h"
#include "../inc/ioooctls.h"
#include "../vmm/inc/devicebus.h"
//#include "../mmio_man.h"

#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <capstone/capstone.h>
#include <pthread.h>
#define CAPSTONE_ARCH CS_ARCH_X86
#define CAPSTONE_MODE CS_MODE_16

#define DEBUG 1


int init_capstone(vcpu_t *vcpu) {
	if (cs_open(CAPSTONE_ARCH, CS_MODE_16, &vcpu->cs_handle) != CS_ERR_OK) {
		printf("ERROR: disassembler failed to initialize.\n");
		return -1;
	}
	cs_option(vcpu->cs_handle, CS_OPT_DETAIL, CS_OPT_ON);
	return 0;
}

int reinit_capstone(vcpu_t *vcpu, uint8_t mode) {
	cs_option(vcpu->cs_handle, CS_OPT_MODE, mode);
	return 0;
}

void update_mode(vcpu_t *vcpu) {
	ooo_state_t *state = (ooo_state_t *)vcpu->comm;
	if (vcpu->last_mode != state->mode) {
		printf("reinitializing capstone\n");
		int capstone_mode = state->mode == MODE_REAL ? CS_MODE_16 : CS_MODE_32;
		reinit_capstone(vcpu, capstone_mode);
		vcpu->last_mode = state->mode;
	}
	return;
}

void guest_unmap(int hyper_fd, __u64 gpa, __u64 map_size)
{
	struct ooo_userspace_memory_region m =
	{
		.guest_phys_addr = gpa,
		.memory_size = map_size,
	};
	ioctl(hyper_fd, OOO_UNMAP, &m);
}

void *guest_map(int hyper_fd, __u64 gpa, __u64 map_size)
{
	void *guest_memory = mmap(0, map_size, PROT_WRITE | PROT_READ, MAP_SHARED, hyper_fd, gpa);
	assert(guest_memory != MAP_FAILED);
	return guest_memory;
}

int get_reg_offset(x86_reg cs_reg) {
	switch(cs_reg) {
		case X86_REG_AL:
		case X86_REG_AX:
		case X86_REG_EAX:
			return 0;

		case X86_REG_CL:
		case X86_REG_CX:
		case X86_REG_ECX:
			return 1;

		case X86_REG_DL:
		case X86_REG_DX:
		case X86_REG_EDX:
			return 2;

		case X86_REG_BL:
		case X86_REG_BX:
		case X86_REG_EBX:
			return 3;

		case X86_REG_SPL:
		case X86_REG_SP:
		case X86_REG_ESP:
			return 4;

		case X86_REG_BPL:
		case X86_REG_BP:
		case X86_REG_EBP:
			return 5;

		case X86_REG_SIL:
		case X86_REG_SI:
		case X86_REG_ESI:
			return 6;

		case X86_REG_DIL:
		case X86_REG_DI:
		case X86_REG_EDI:
			return 7;

		case X86_REG_R8B:
		case X86_REG_R8W:
		case X86_REG_R8D:
			return 8;

		case X86_REG_R9B:
		case X86_REG_R9W:
		case X86_REG_R9D:
			return 9;

		case X86_REG_R10B:
		case X86_REG_R10W:
		case X86_REG_R10D:
			return 10;

		case X86_REG_R11B:
		case X86_REG_R11W:
		case X86_REG_R11D:
			return 11;

		case X86_REG_R12B:
		case X86_REG_R12W:
		case X86_REG_R12D:
			return 12;

		case X86_REG_R13B:
		case X86_REG_R13W:
		case X86_REG_R13D:
			return 13;

		case X86_REG_R14B:
		case X86_REG_R14W:
		case X86_REG_R14D:
			return 14;

		case X86_REG_R15B:
		case X86_REG_R15W:
		case X86_REG_R15D:
			return 15;

		default: 
			//printf("\n\nTHAT'S AMORAY\n\n");
			return -1;

	}
}

int get_capstone_mode(int ooo_mode) {
	switch (ooo_mode) {
		case MODE_REAL:
			return CS_MODE_16;
		case MODE_PROTECTED:
			return CS_MODE_32;
		case MODE_PAGING:
			return CS_MODE_64;
		default:
			return -1;
	}
}

int debug_step(vcpu_t *vcpu) {
	void *guest_mem = vcpu->hv->guest_mem;
	ooo_state_t *state = vcpu->comm;
	uint64_t instr_bytes[2] = {0};
	printf("guest_memory: 0x%llx\n", (__u64)guest_mem);
	printf("rip_gpa: 0x%llx\n", (__u64)(guest_mem+state->rip_gpa));
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), sizeof(instr_bytes));
	void *shellcode_addr = 0;
	uint64_t shellcode_size = 16;
	cs_insn *insn;
	size_t count;
	cs_option(vcpu->cs_handle, CS_OPT_DETAIL, CS_OPT_ON);
	count = cs_disasm(vcpu->cs_handle, (void *)instr_bytes, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
	if (count == 0)
	{
		printf("Failed to disassemble!");
	}

	printf("Addr: 0x%016lx", (unsigned long)insn[0].address);
	puts("");
	printf("Bytes: ");
	for (int k = 0; k < insn[0].size; k++) printf("%02hhx ", insn[0].bytes[k]);
	puts("");
	printf("Instruction: %s %s\n", insn[0].mnemonic, insn[0].op_str);
	puts("");
	// NOTE: We're responsible for updating the rip since we're decoding
	state->regs.rip += insn[0].size;
	return 0;
}

int handle_pio(vcpu_t *vcpu) {
	ooo_state_t *state = vcpu->comm;
	int ret = 0;
	uint64_t data = 0;
	uint16_t port = 0;
	uint8_t direction = 0;

	void *guest_mem = vcpu->hv->guest_mem;
	uint64_t instr_bytes[2] = {0};
	size_t count = 0;
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), 16);
	cs_insn *insn;

	count = cs_disasm(vcpu->cs_handle, (uint8_t *)instr_bytes, 16, 0, 0, &insn);
	if (count == 0) {
		printf("Failed to disassemble!");
		return -1;
	}

	cs_insn blah = insn[0];
	cs_detail *detail = (&blah)->detail;
	printf("Instruction: %s %s\n", insn[0].mnemonic, insn[0].op_str);

	if (strncmp(blah.mnemonic, "out", 3) == 0) {
		direction = IO_DIRECTION_OUT;
	}
	else if (strncmp(blah.mnemonic, "in", 2) == 0) {
		direction = IO_DIRECTION_IN;
	}
	else {
		printf("WHAT THE FUCK KIND OF IO ARE YOU??\n");
	}

	// OUT FIRST
	// XXX out <port>, <data>
	if (direction == IO_DIRECTION_OUT) {
		// okay so we want operand 0
		cs_x86_op *port_op = &(detail->x86.operands[0]);
		// let's do port first
		if (port_op->type == X86_OP_IMM) {
			port = port_op->imm;
		}
		else if (port_op->type == X86_OP_REG) {
			// BUG: Not checking ret val
			port = (uint16_t)state->regs.rdx;
		}
		else {
			printf("MAN WHAT THE FUCK KIND OF IO IS THIS\n\n");
		}

		// now do data --> always a reg
		cs_x86_op *data_op = &(detail->x86.operands[1]);
		assert (data_op->size <= 8);
		memcpy(&data, &state->regs.rax, data_op->size);
		printf("OUT port: 0x%x   data: 0x%lx   size: 0x%x\n", port, data, data_op->size);
		ret = dbusHandlePioAccess(vcpu, port, (uint8_t *)&data, direction, data_op->size,	0);
	}

	// IN
	// IN <Dst>, <Port>
	else {
		cs_x86_op *port_op = &(detail->x86.operands[1]);
		// let's do port first
		if (port_op->type == X86_OP_IMM) {
			port = port_op->imm;
		}
		else if (port_op->type == X86_OP_REG) {
			// BUG: Not checking ret val
			port = (uint16_t)state->regs.rdx;
		}

		// now do data --> always a reg
		cs_x86_op *data_op = &(detail->x86.operands[0]);
		assert (data_op->size <= 8);
		printf("IN port: 0x%x   data: 0x%lx size: 0x%x\n", port, data, data_op->size);
		ret = dbusHandlePioAccess(vcpu, port, (uint8_t *)&data, direction, data_op->size,	0);
		memcpy(&state->regs.rax, &data, data_op->size);
	}

	// NOTE: We're responsible for updating the rip since we're decoding
	state->regs.rip += blah.size;

	cs_free(insn, count);
	return ret;
}

int handle_mmio(vcpu_t *vcpu) {
	ooo_state_t *state = vcpu->comm;
	int ret = 0;
	uint64_t data = 0;
	void *guest_mem = vcpu->hv->guest_mem;
	uint64_t instr_bytes[2] = {0};
	size_t count = 0;
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), 16);
	cs_insn *insn;

	count = cs_disasm(vcpu->cs_handle, (uint8_t *)instr_bytes, 16, 0, 0, &insn);
	if (count == 0) {
		printf("Failed to disassemble!");
		return -1;
	}

	cs_insn blah = insn[0];
	cs_detail *detail = (&blah)->detail;

	if (strncmp(blah.mnemonic, "mov", 3)) {
		printf("\n[!] Not a mov!!\n");
		printf("Instruction: %s %s\n", insn[0].mnemonic, insn[0].op_str);
		printf("fault addr: 0x%llx\n", state->mmio.phys_addr);
		printf("eax: 0x%llx  ebx: 0x%llx   esp: 0x%llx  ecx: 0x%llx\n", state->regs.rax, state->regs.rbx, state->regs.rsp, state->regs.rsp);
		assert(false);
	}

	// reads
	// XXX reg, [blah]
	if (!state->mmio.is_write) {
		// okay so we want operand 0
		cs_x86_op *op = &(detail->x86.operands[0]);
		// BUG: not checking return value
		int reg_offset = get_reg_offset(op->reg);
		assert (op->size <= 8);
		ret = dbusHandleMmioAccess(vcpu, state->mmio.phys_addr, &data, op->size, state->mmio.is_write);
		memcpy(((uint8_t *)&((uint64_t *)&state->regs)[reg_offset]), &data, op->size);
	}

	// writes
	// XXX [blah], reg
	else {
		// okay so we want operand 1
		cs_x86_op *op = &(detail->x86.operands[1]);
		// BUG: not checking return value
		int reg_offset = get_reg_offset(op->reg);
		assert (op->size <= 8);
		memcpy(&data, ((uint8_t *)&((uint64_t *)&state->regs)[reg_offset]), op->size);
		ret = dbusHandleMmioAccess(vcpu, state->mmio.phys_addr, &data, op->size, state->mmio.is_write);
	}

	// NOTE: We're responsible for updating the rip since we're decoding
	state->regs.rip += blah.size;

	cs_free(insn, count);
	return ret;
}


/*
hv_t *hvInitHypervisor(void);
int hvCreateInterruptController(hv_t *);
int hvCreateVcpu(hv_t *, vcpu_t *);
int hvEstablishComm(vcpu_t *);
int hvSetVcpuRegisters(vcpu_t *);
int hvSetCpuid(vcpu_t *);
int hvSetMemory(hv_t *, void *, size_t, uint64_t, bool);
void hvDelMemory(hv_t *, int);
int hvRunVcpu(vcpu_t *);
int waitForSipi(vcpu_t *);
*/

void hvDelMemory(hv_t *hv, int man) {
	return;
}

int hvSetCpuid(vcpu_t *vcpu) {
	return 0;
}

int hvSetVcpuRegisters(vcpu_t *vcpu) {
	ooo_state_t *state = (ooo_state_t *)vcpu->comm;
	struct ooo_regs *r = &(state->regs);

  r->rax = vcpu->regs.gpr[G_EAX];
  r->rbx = vcpu->regs.gpr[G_EBX];
  r->rcx = vcpu->regs.gpr[G_ECX];
  r->rdx = vcpu->regs.gpr[G_EDX];
  r->rsi = vcpu->regs.gpr[G_ESI];
  r->rdi = vcpu->regs.gpr[G_EDI];
  r->rsp = vcpu->regs.gpr[G_ESP];
  r->rbp = vcpu->regs.gpr[G_EBP];

  r->rflags = vcpu->regs.eflags;
  r->rip = vcpu->regs.eip;

	// we set up
	return 0;
}

int hvGetVcpuRegisters(vcpu_t *vcpu) {
	ooo_state_t *state = (ooo_state_t *)vcpu->comm;
	struct ooo_regs *r = &(state->regs);

  vcpu->regs.gpr[G_EAX] = r->rax;
  vcpu->regs.gpr[G_EBX] = r->rbx;
  vcpu->regs.gpr[G_ECX] = r->rcx;
  vcpu->regs.gpr[G_EDX] = r->rdx;
  vcpu->regs.gpr[G_ESI] = r->rsi;
  vcpu->regs.gpr[G_EDI] = r->rdi;
  vcpu->regs.gpr[G_ESP] = r->rsp;
  vcpu->regs.gpr[G_EBP] = r->rbp;

  vcpu->regs.eflags = r->rflags;
  vcpu->regs.eip = r->rip;
	return 0;
}

int hvCreateVcpu(hv_t *hv, vcpu_t *vcpu) {
	// initialize our capstone disassembler
	init_capstone(vcpu);
	// start in real mode
	vcpu->last_mode = CS_MODE_16;
	// all vcpus have the same fd..
	return hv->fd;
}
int hvEstablishComm(vcpu_t *vcpu) {
	// create a state for this gentlevcpu
	ooo_state_t *state = calloc(1, sizeof(ooo_state_t));
	if (!state)
		return -1;
	vcpu->comm = (void *)state;
	return 0;
}

hv_t *hvInitHypervisor(void) {
  hv_t *hv = NULL;
  hv = calloc(1,sizeof(hv_t));
  if (hv == NULL) {
    return NULL;
  }
	hv->fd = open("/dev/hyper-o", O_RDWR);
	if (hv->fd < 0) {
		free(hv);
		perror("hyper-o open: ");
		return NULL;
	}
	hv->guest_mem = (char *)guest_map(hv->fd, 0, 0x400000);
	// create vram hole
	guest_unmap(hv->fd, 0xA0000, 0xC0000-0xA0000); \
  pthread_mutex_init(&hv->bus_access_mutex, NULL);
	return hv;
}

int hvSetMemory(hv_t *hv, void *hva, size_t len, uint64_t gpa, bool readonly) {
	switch (len) {
		case HOST_SYS_MEM_SIZE:
			hv->sys_mem = hv->guest_mem + GUEST_SYS_MEM_PADDR;
			break;

		case HOST_FW_SIZE:
			hv->fw = hv->guest_mem + GUEST_FW_PADDR;
			break;

		case HOST_BIOS_SIZE:
			memcpy(hv->guest_mem+GUEST_BIOS_PADDR+0x3c000, hva+0x3c000, 0x4000);
			hv->bios_rom = hv->guest_mem + GUEST_BIOS_PADDR;
			break;

		default:
			return -1;
	}
	return 0;
}

int hvRunVcpu(vcpu_t *vcpu) {
  int ret = 0;
  int run_ret = 0;
  struct ooo_state_t *state = vcpu->comm;
	// TODO TODO TODO ONLY DO FOR BSP (????)
	state->regs.rsp = 0;
	state->regs.rip = 0xfff0;
	vcpu->dirty = false;

  do {

    //if (vcpu->dirty) {
    //  hvSetVcpuRegisters(vcpu);
    //  vcpu->dirty = false;
    //}

    // check if there are interrupts that need injecting
    //checkAndSendInterrupt(vcpu->hv, vcpu);

    run_ret = ioctl(vcpu->driver_fd, OOO_RUN, vcpu->comm);

		// update mode on exit
		update_mode(vcpu);
    // update our internal vcpu state after the vmexit
    //hvGetVcpuRegisters(vcpu);

		//printf("\n\n RUN RET: %d\n\n", run_ret);
    switch (run_ret) {
    case EXITCOOODE_HLT:
      if (DEBUG) {
        printf("vcpu %d halted at eip: 0x%llx\n", vcpu->id, state->regs.rip);
      }
      vcpu->state = STATE_HALTED;
      ret = VM_SHUTDOWN;
			break;
      //ret = waitForSipi(vcpu);
      //break;
    case EXITCOOODE_SHUTDOWN:
      ret = VM_SHUTDOWN;
      break;
    case EXITCOOODE_IO:
      /* handle io here */
			ret = handle_pio(vcpu);
      break;
    case EXITCOOODE_EPT_VIOLATION:
      /* handle mmio here */
			//printf("\n\nHANDLING MMIO\n\n");
			ret = handle_mmio(vcpu);
			if (ret == -1) {
				printf("rip: 0%llx\n", ((ooo_state_t *)vcpu->comm)->rip_gpa);
				printf("esi: 0%llx\n", ((ooo_state_t *)vcpu->comm)->regs.rsi);
			}
      break;
		case EXITCOOODE_TRAP_FLAG:
			ret = debug_step(vcpu);
			break;
    default:
      if (DEBUG) {
        printf("exit code: %d\n", run_ret);
      }
      ret = VM_UNHANDLED_EXIT;
      break;
    }
  } while(!ret);
	printf("\nEXIT REASON WAS: %d\n", run_ret);
	printf("rip: 0%llx\n", ((ooo_state_t *)vcpu->comm)->rip_gpa);
	return 0;
}
