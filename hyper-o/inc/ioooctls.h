#pragma once

#include <asm-generic/int-ll64.h>

#define MODE_REAL 0
#define MODE_PROTECTED 1
#define MODE_PAGING 2


// yes
#define OOO_RUN    0x00133700
#define OOO_PEEK   0x03133700
#define OOO_POKE   0x31333700
#define OOO_MAP    0x33133700
#define OOO_UNMAP  0x13333700

/* Architectural interrupt line count. */
#define OOO_NR_INTERRUPTS 256

#define MAX_VCPUS 8
#define MAX_PCPUS 8

struct ooo_userspace_memory_region
{
	__u32 ooo;
	__u32 oooooo;
	__u64 guest_phys_addr;
	__u64 memory_size; /* bytes */
	__u64 userspace_addr; /* start of the userspace allocated memory */
};


/* When set in flags, include corresponding fields on OOO_SET_VCPU_EVENTS */
#define OOO_VCPUEVENT_VALID_NMI_PENDING	0x00000001
#define OOO_VCPUEVENT_VALID_SIPI_VECTOR	0x00000002
#define OOO_VCPUEVENT_VALID_SHADOW	0x00000004
#define OOO_VCPUEVENT_VALID_SMM		0x00000008
#define OOO_VCPUEVENT_VALID_PAYLOAD	0x00000010

/* Interrupt shadow states */
#define OOO_X86_SHADOW_INT_MOV_SS	0x01
#define OOO_X86_SHADOW_INT_STI		0x02


struct ooo_debug_exit_arch
{
	__u32 exception;
	__u32 pad;
	__u64 pc;
	__u64 dr6;
	__u64 dr7;
};


#pragma pack(push, 1)
struct ooo_regs
{
	__u64 rax;
	__u64 rcx;
	__u64 rdx;
	__u64 rbx;
	__u64 rsp;
	__u64 rbp;
	__u64 rsi;
	__u64 rdi;
	__u64 r8;
	__u64 r9;
	__u64 r10;
	__u64 r11;
	__u64 r12;
	__u64 r13;
	__u64 r14;        // 70h
	__u64 r15;
	__u64 rip, rflags;
};
#pragma pack(pop)


struct ooo_segment
{
	__u64 base;
	__u32 limit;
	__u16 selector;
	__u8  type;
	__u8  present, dpl, db, s, l, g, avl;
	__u8  unusable;
	__u8  padding;
};

struct ooo_dtable
{
	__u64 base;
	__u16 limit;
	__u16 padding[3];
};


/* for OOO_GET_SREGS and OOO_SET_SREGS */
struct ooo_sregs
{
	/* out (OOO_GET_SREGS) / in (OOO_SET_SREGS) */
	struct ooo_segment cs, ds, es, fs, gs, ss;
	struct ooo_segment tr, ldt;
	struct ooo_dtable gdt, idt;
	__u64 cr0, cr2, cr3, cr4, cr8;
	__u64 efer;
	__u64 apic_base;
	__u64 interrupt_bitmap[(OOO_NR_INTERRUPTS + 63) / 64];
};

/* OOO_EXIT_IO */
struct pio_t
{
	__u8 direction;
	__u8 size; /* bytes */
	__u16 port;
	__u32 count;
	__u8 data[8]; /* relative to ooo_state start */ // TODO: TODO: NOTICE: DIFFERENT FROM KVM
};

#define OOO_EXIT_IO_IN  0
#define OOO_EXIT_IO_OUT 1
#define OOO_SYSTEM_EVENT_SHUTDOWN       1
#define OOO_SYSTEM_EVENT_RESET          2
#define OOO_SYSTEM_EVENT_CRASH          3

typedef struct ooo_state_t
{
	/* in */
	__u8 request_interrupt_window;
	__u8 immediate_exit;

	/* in (pre_ooo_state), out (post_ooo_state) */
	__u64 cr8;
	__u64 apic_base;

	__u8 mode;
	__u64 rip_gpa;
	__u64 apicbase;

	union
	{
		/* OOO_EXIT_UNKNOWN */
		struct
		{
			__u64 hardware_exit_reason;
		} hw;
		/* OOO_EXIT_FAIL_ENTRY */
		struct
		{
			__u64 hardware_entry_failure_reason;
		} fail_entry;
		/* OOO_EXIT_EXCEPTION */
		struct
		{
			__u32 exception;
			__u32 error_code;
		} ex;
		/* OOO_EXIT_IO */
		struct
		{
			__u8 direction;
			__u8 size; /* bytes */
			__u16 port;
			__u32 count;
			__u8 data[8]; /* relative to ooo_state start */ // TODO: TODO: NOTICE: DIFFERENT FROM KVM
		} io;
		/* OOO_EXIT_DEBUG */
		struct
		{
			struct ooo_debug_exit_arch arch;
		} debug;
		/* OOO_EXIT_MMIO */
		struct
		{
			__u64 phys_addr;
			__u8  is_write;
		} mmio;
		/* OOO_EXIT_HYPERCALL */
		struct
		{
			__u64 nr;
			__u64 args[6];
			__u64 ret;
			__u32 longmode;
			__u32 pad;
		} hypercall;
		/* OOO_EXIT_TPR_ACCESS */
		struct
		{
			__u64 rip;
			__u32 is_write;
			__u32 pad;
		} tpr_access;
		/* OOO_EXIT_INTERNAL_ERROR */
		struct
		{
			__u32 suberror;
			/* Available with OOO_CAP_INTERNAL_ERROR_DATA: */
			__u32 ndata;
			__u64 data[16];
		} internal;
		/* OOO_EXIT_OSI */
		struct
		{
			__u64 gprs[32];
		} osi;
		/* OOO_EXIT_PAPR_HCALL */
		struct
		{
			__u64 nr;
			__u64 ret;
			__u64 args[9];
		} papr_hcall;
		/* OOO_EXIT_EPR */
		struct
		{
			__u32 epr;
		} epr;
		/* OOO_EXIT_SYSTEM_EVENT */
		struct
		{
			__u32 type;
			__u64 flags;
		} system_event;
		/* OOO_EXIT_IOAPIC_EOI */
		struct
		{
			__u8 vector;
		} eoi;
		/* Fix the size of the union. */
		char padding[256];
	};

	__u64 fart;
	struct ooo_regs regs;
	struct ooo_sregs sregs;
	//struct ooo_vcpu_events events;
} ooo_state_t;

/* VMX basic exit reasons. */
#define EXITCOOODE_EXC_NMI			0
#define EXITCOOODE_EXT_INT			1
#define EXITCOOODE_SHUTDOWN			2
#define EXITCOOODE_INIT			3
#define EXITCOOODE_SIPI			4
#define EXITCOOODE_SMI			5
#define EXITCOOODE_OTHER_SMI			6
#define EXITCOOODE_INT_WINDOW		7
#define EXITCOOODE_NMI_WINDOW		8
#define EXITCOOODE_TASK_SWITCH		9
#define EXITCOOODE_CPUID			10
#define EXITCOOODE_GETSEC			11
#define EXITCOOODE_HLT			12
#define EXITCOOODE_INVD			13
#define EXITCOOODE_INVLPG			14
#define EXITCOOODE_RDPMC			15
#define EXITCOOODE_RDTSC			16
#define EXITCOOODE_RSM			17
#define EXITCOOODE_VMCALL			18
#define EXITCOOODE_VMCLEAR			19
#define EXITCOOODE_VMLAUNCH			20
#define EXITCOOODE_VMPTRLD			21
#define EXITCOOODE_VMPTRST			22
#define EXITCOOODE_VMREAD			23
#define EXITCOOODE_VMRESUME			24
#define EXITCOOODE_VMWRITE			25
#define EXITCOOODE_VMXOFF			26
#define EXITCOOODE_VMXON			27
#define EXITCOOODE_CR			28
#define EXITCOOODE_DR			29
#define EXITCOOODE_IO			30
#define EXITCOOODE_RDMSR			31
#define EXITCOOODE_WRMSR			32
#define EXITCOOODE_FAIL_GUEST_INVALID	33
#define EXITCOOODE_FAIL_MSR_INVALID		34
#define EXITCOOODE_MWAIT			36
#define EXITCOOODE_TRAP_FLAG			37
#define EXITCOOODE_MONITOR			39
#define EXITCOOODE_PAUSE			40
#define EXITCOOODE_FAIL_MACHINE_CHECK	41
#define EXITCOOODE_TPR_BELOW			43
#define EXITCOOODE_APIC_ACCESS		44
#define EXITCOOODE_VEOI			45
#define EXITCOOODE_GDTR_IDTR			46
#define EXITCOOODE_LDTR_TR			47
#define EXITCOOODE_EPT_VIOLATION		48
#define EXITCOOODE_EPT_MISCONFIG		49
#define EXITCOOODE_INVEPT			50
#define EXITCOOODE_RDTSCP			51
#define EXITCOOODE_PREEMPT_TIMEOUT		52
#define EXITCOOODE_INVVPID			53
#define EXITCOOODE_WBINVD			54
#define EXITCOOODE_XSETBV			55
#define EXITCOOODE_APIC_WRITE		56
#define EXITCOOODE_RDRAND			57
#define EXITCOOODE_INVPCID			58
#define EXITCOOODE_VMFUNC			59
#define EXITCOOODE_ENCLS			60
#define EXITCOOODE_RDSEED			61
#define EXITCOOODE_PAGE_LOG_FULL		62
#define EXITCOOODE_XSAVES			63
#define EXITCOOODE_XRSTORS			64
#define EXITCOOODE_SPP			66
#define EXITCOOODE_UMWAIT			67
#define EXITCOOODE_TPAUSE			68
