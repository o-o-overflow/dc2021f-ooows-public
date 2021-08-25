#include "mmio_man.h"
#include "inc/ioooctls.h"
//#include "inc/vmexit.h"
//#include "vmm/inc/devicebus.h"


#include <assert.h>
#include <capstone/capstone.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#define CAPSTONE_ARCH CS_ARCH_X86
#define CAPSTONE_MODE CS_MODE_16


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
			return -1;

	}
}


int handle_pio2(ooo_state_t *state, void *guest_mem) {
	uint64_t instr_bytes[2] = {0};
	uint64_t data = 0;
	uint16_t port = 0;
	uint8_t size = 0;
	printf("guest_memory: 0x%llx\n", (__u64)guest_mem);
	printf("rip_gpa: 0x%llx\n", (__u64)(guest_mem+state->rip_gpa));
	printf("rip_hva: 0x%llx\n\n\n", (__u64)(guest_mem+state->rip_gpa));
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), sizeof(instr_bytes));
	int capstone_mode = get_capstone_mode(state->mode);
	void *shellcode_addr = 0;
	uint64_t shellcode_size = 16;
	csh handle;
	cs_insn *insn;
	size_t count;
	if (cs_open(CAPSTONE_ARCH, capstone_mode, &handle) != CS_ERR_OK) {
    sleep(1);
		printf("ERROR: disassembler failed to initialize.\n");
    sleep(1);
		return 1;
	}
	cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
	count = cs_disasm(handle, instr_bytes, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
	if (count == 0)
	{
    sleep(1);
		printf("Failed to disassemble!");
	}

  sleep(1);
	printf("Addr: 0x%016lx", (unsigned long)insn[0].address);
	puts("");
	printf("Bytes: ");
	for (int k = 0; k < insn[0].size; k++) printf("%02hhx ", insn[0].bytes[k]);
	puts("");
	printf("Instruction: %s %s\n", insn[0].mnemonic, insn[0].op_str);
	puts("");
	cs_insn blah = insn[0];
	printf("blah.id: 0x%x\n", blah.id);
	printf("blah.mnemonic: %s\n", blah.mnemonic);
	printf("blah.op_str: %s\n", blah.op_str);
  sleep(1);
	cs_detail *detail = (&blah)->detail;
	printf("regs_read: %d\n", detail->regs_read_count);
	printf("regs write: %d\n", detail->regs_write_count);
	printf("op count: %d\n", detail->x86.op_count);
	int n =0;
	for (n=0; n < detail->x86.op_count; n++) {
		cs_x86_op *op = &(detail->x86.operands[n]);
		if (op->type == X86_OP_REG) {
			printf("op->size: %d  op->reg: %d\n", op->size, op->reg);
		}
	}

	struct io_request io_req = {0};
	io_req.type = IOTYPE_MMIO;
	io_req.mmio.is_write = state->mmio.is_write;
	io_req.mmio.phys_addr = state->mmio.phys_addr;

	// OUT FIRST
	// XXX out <port>, <data>
	if (state->io.direction == 1) {
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

		// now do data --> always a reg
		cs_x86_op *data_op = &(detail->x86.operands[1]);
		assert (data_op->size <= 8);
    size = data_op->size;
		memcpy(&data, &state->regs.rax, data_op->size);
		//printf("OUT port: 0x%x   data: 0x%lx   size: 0x%x\n", port, data, size);

	}

	// IN NOW
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
    size = data_op->size;
		printf("IN port: 0x%x   size: 0x%x\n", port, data, size);
		memcpy(&data, &state->regs.rax, data_op->size);
	}

	// NOTE: We're responsible for updating the rip since we're decoding
	printf("instr size: %d\n", blah.size);
	state->regs.rip += blah.size;

	cs_free(insn, count);
	cs_close(&handle);
	return 0;
	
}

int handle_pio(ooo_state_t *state, void *guest_mem) {
	uint64_t instr_bytes[2] = {0};
  sleep(1);
	printf("guest_memory: 0x%llx\n", (__u64)guest_mem);
	printf("rip_gpa: 0x%llx\n", (__u64)(guest_mem+state->rip_gpa));
	printf("rip_hva: 0x%llx\n\n\n", (__u64)(guest_mem+state->rip_gpa));
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), sizeof(instr_bytes));
	int capstone_mode = get_capstone_mode(state->mode);
	void *shellcode_addr = 0;
	uint64_t shellcode_size = 16;
	csh handle;
	cs_insn *insn;
	size_t count;
	if (cs_open(CAPSTONE_ARCH, capstone_mode, &handle) != CS_ERR_OK) {
    sleep(1);
		printf("ERROR: disassembler failed to initialize.\n");
    sleep(1);
		return 1;
	}
	cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
	count = cs_disasm(handle, instr_bytes, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
	if (count == 0)
	{
    sleep(1);
		printf("Failed to disassemble!");
	}

  //sleep(1);
	//printf("Addr: 0x%016lx", (unsigned long)insn[0].address);
	//puts("");
	//printf("Bytes: ");
	//for (int k = 0; k < insn[0].size; k++) printf("%02hhx ", insn[0].bytes[k]);
	//puts("");
	//printf("Instruction: %s %s\n", insn[0].mnemonic, insn[0].op_str);
	//puts("");
	cs_insn blah = insn[0];
	//printf("blah.id: 0x%x\n", blah.id);
	//printf("blah.mnemonic: %s\n", blah.mnemonic);
	//printf("blah.op_str: %s\n", blah.op_str);
  //sleep(1);
	cs_detail *detail = (&blah)->detail;
	//printf("regs_read: %d\n", detail->regs_read_count);
	//printf("regs write: %d\n", detail->regs_write_count);
	//printf("op count: %d\n", detail->x86.op_count);
	int n =0;
	for (n=0; n < detail->x86.op_count; n++) {
		cs_x86_op *op = &(detail->x86.operands[n]);
		if (op->type == X86_OP_REG) {
			printf("op->size: %d  op->reg: %d\n", op->size, op->reg);
		}
	}

	struct io_request io_req = {0};
	io_req.type = IOTYPE_MMIO;
	io_req.mmio.is_write = state->mmio.is_write;
	io_req.mmio.phys_addr = state->mmio.phys_addr;

	// OUT FIRST
	// XXX out <port>, <data>
	if (state->io.direction == 1) {
		printf("\n\n OUT <PORt> <DATA>\n\n");
		// okay so we want operand 0
		//cs_x86 *op = &(detail->x86.operands[0]);
		cs_x86_op *op = &(detail->x86.operands[0]);
		if (op->type == X86_OP_IMM) {
			printf("operand 0 (port) is imm: 0x%lx\n", op->imm);
		}
    else if (op->type == X86_OP_REG) {
			printf("operand 0 (port) is reg\n");
    }
		io_req.mmio.len = op->size;
		uint64_t data = 0x1337;
		int reg_offset = get_reg_offset(op->reg);
		assert (op->size <= 8);

	}

	// IN NOW
	else {
		printf("\n\n IN <DST>, <PORT>\n\n");
		// okay so we want operand 1
		cs_x86_op *op = &(detail->x86.operands[1]);
		//io_req.mmio.len = op->size;
		//int reg_offset = get_reg_offset(op->reg);
		//assert (op->size <= 8);
		//io_req.mmio.data = 0;
		//memcpy(&io_req.mmio.data, ((uint8_t *)&((uint64_t *)&state->regs)[reg_offset]), op->size);
	}

	// NOTE: We're responsible for updating the rip since we're decoding
	printf("instr size: %d\n", blah.size);
	state->regs.rip += blah.size;

	cs_free(insn, count);
	cs_close(&handle);
	return 0;
	
	return 0;
}

int debug_step(ooo_state_t *state, void *guest_mem) {
	uint64_t instr_bytes[2] = {0};
	printf("guest_memory: 0x%llx\n", (__u64)guest_mem);
	printf("rip_gpa: 0x%llx\n", (__u64)(guest_mem+state->rip_gpa));
	printf("rip_hva: 0x%llx\n\n\n", (__u64)(guest_mem+state->rip_gpa));
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), sizeof(instr_bytes));
	int capstone_mode = get_capstone_mode(state->mode);
	void *shellcode_addr = 0;
	uint64_t shellcode_size = 16;
	csh handle;
	cs_insn *insn;
	size_t count;
	if (cs_open(CAPSTONE_ARCH, capstone_mode, &handle) != CS_ERR_OK) {
		printf("ERROR: disassembler failed to initialize.\n");
		return 1;
	}
	cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
	count = cs_disasm(handle, (void *)instr_bytes, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
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

int handle_mmio(ooo_state_t *state, void *guest_mem) {
	uint64_t instr_bytes[2] = {0};
	printf("guest_memory: 0x%llx\n", (__u64)guest_mem);
	printf("rip_gpa: 0x%llx\n", (__u64)(guest_mem+state->rip_gpa));
	printf("rip_hva: 0x%llx\n\n\n", (__u64)(guest_mem+state->rip_gpa));
	memcpy(instr_bytes, guest_mem+(state->rip_gpa), sizeof(instr_bytes));
	int capstone_mode = get_capstone_mode(state->mode);
	apply_mmio(state, instr_bytes, capstone_mode);
	
	return 0;
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

//int apply_mmio(ooo_state_t *state, uint32_t mode) {
int apply_mmio(ooo_state_t *state, void *bytes, int capstone_mode) {
	void *shellcode_addr = 0;
	uint64_t shellcode_size = 16;
	csh handle;
	cs_insn *insn;
	size_t count;
	if (cs_open(CAPSTONE_ARCH, capstone_mode, &handle) != CS_ERR_OK) {
		printf("ERROR: disassembler failed to initialize.\n");
		return 1;
	}
	cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
	count = cs_disasm(handle, bytes, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
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
	cs_insn blah = insn[0];
	printf("blah.id: 0x%x\n", blah.id);
	printf("blah.mnemonic: %s\n", blah.mnemonic);
	printf("blah.op_str: %s\n", blah.op_str);
	cs_detail *detail = (&blah)->detail;
	printf("regs_read: %d\n", detail->regs_read_count);
	printf("regs write: %d\n", detail->regs_write_count);
	printf("op count: %d\n", detail->x86.op_count);
	int n =0;
	for (n=0; n < detail->x86.op_count; n++) {
		cs_x86_op *op = &(detail->x86.operands[n]);
		if (op->type == X86_OP_REG) {
			printf("op->size: %d  op->reg: %d\n", op->size, op->reg);
		}
	}

	struct io_request io_req = {0};
	io_req.type = IOTYPE_MMIO;
	io_req.mmio.is_write = state->mmio.is_write;
	io_req.mmio.phys_addr = state->mmio.phys_addr;

	// reads
	// XXX reg, [blah]
	if (!state->mmio.is_write) {
		printf("\n\n mov REG, [BLAH]\n\n");
		// okay so we want operand 0
		//cs_x86 *op = &(detail->x86.operands[0]);
		cs_x86_op *op = &(detail->x86.operands[0]);
		io_req.mmio.len = op->size;
		uint64_t data = 0x1337;
		int reg_offset = get_reg_offset(op->reg);
		assert (op->size <= 8);
		state->fart = 0;
		printf("state->regs: %p\n", &state->regs);
		printf("state->regs[-1]: %p\n", (uint8_t *)&((uint64_t *)&state->regs)[reg_offset]);
		memcpy(((uint8_t *)&((uint64_t *)&state->regs)[reg_offset]), &data, op->size);
		printf("\nfart: %d\n", state->fart);

	}

	// writes
	// XXX [blah], reg
	else {
		printf("\n\n mov [BLAH], REG\n\n");
		// okay so we want operand 1
		cs_x86_op *op = &(detail->x86.operands[1]);
		io_req.mmio.len = op->size;
		int reg_offset = get_reg_offset(op->reg);
		assert (op->size <= 8);
		io_req.mmio.data = 0;
		memcpy(&io_req.mmio.data, ((uint8_t *)&((uint64_t *)&state->regs)[reg_offset]), op->size);
	}

	// NOTE: We're responsible for updating the rip since we're decoding
	printf("instr size: %d\n", blah.size);
	state->regs.rip += blah.size;

	cs_free(insn, count);
	cs_close(&handle);
	return 0;
}
