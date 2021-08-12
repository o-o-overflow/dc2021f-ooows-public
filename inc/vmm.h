#ifndef _VMM_H_
#define _VMM_H_

#include <stdint.h>
#include <stdbool.h>

enum {
      G_EAX = 0,
      G_ECX = 1,
      G_EDX = 2,
      G_EBX = 3,
      G_ESP = 4,
      G_EBP = 5,
      G_ESI = 6,
      G_EDI = 7
};

enum {
      S_ES = 0,
      S_CS = 1,
      S_SS = 2,
      S_DS = 3,
      S_FS = 4,
      S_GS = 5,
      S_LDTR = 6,
      S_TR = 7,
};

/* segment descriptor fields */
#define DESC_G_SHIFT    23
#define DESC_G_MASK     (1 << DESC_G_SHIFT)
#define DESC_B_SHIFT    22
#define DESC_B_MASK     (1 << DESC_B_SHIFT)
#define DESC_L_SHIFT    21 /* x86_64 only : 64 bit code segment */
#define DESC_L_MASK     (1 << DESC_L_SHIFT)
#define DESC_AVL_SHIFT  20
#define DESC_AVL_MASK   (1 << DESC_AVL_SHIFT)
#define DESC_P_SHIFT    15
#define DESC_P_MASK     (1 << DESC_P_SHIFT)
#define DESC_DPL_SHIFT  13
#define DESC_DPL_MASK   (3 << DESC_DPL_SHIFT)
#define DESC_S_SHIFT    12
#define DESC_S_MASK     (1 << DESC_S_SHIFT)
#define DESC_TYPE_SHIFT 8
#define DESC_TYPE_MASK  (15 << DESC_TYPE_SHIFT)
#define DESC_A_MASK     (1 << 8)

#define DESC_CS_MASK    (1 << 11) /* 1=code segment 0=data segment */
#define DESC_C_MASK     (1 << 10) /* code: conforming */
#define DESC_R_MASK     (1 << 9)  /* code: readable */

#define DESC_E_MASK     (1 << 10) /* data: expansion direction */
#define DESC_W_MASK     (1 << 9)  /* data: writable */

#define DESC_TSS_BUSY_MASK (1 << 9)

#ifdef X86_64
#define CPU_NB_REGS 16
#else
#define CPU_NB_REGS 8
#endif

#define MSR_IA32_TSC                    0x10
#define MSR_IA32_APICBASE               0x1b
#define MSR_IA32_APICBASE_BSP           (1<<8)
#define MSR_IA32_APICBASE_ENABLE        (1<<11)
#define MSR_IA32_APICBASE_EXTD          (1 << 10)
#define MSR_IA32_APICBASE_BASE          (0xfffffU<<12)
#define MSR_IA32_FEATURE_CONTROL        0x0000003a
#define MSR_TSC_ADJUST                  0x0000003b
#define MSR_IA32_SPEC_CTRL              0x48
#define MSR_VIRT_SSBD                   0xc001011f
#define MSR_IA32_PRED_CMD               0x49
#define MSR_IA32_UCODE_REV              0x8b
#define MSR_IA32_CORE_CAPABILITY        0xcf

#define MSR_IA32_SYSENTER_CS            0x174
#define MSR_IA32_SYSENTER_ESP           0x175
#define MSR_IA32_SYSENTER_EIP           0x176

#define MSR_PAT                         0x277

#define APIC_DEFAULT_ADDRESS 0xfee00000
#define APIC_SPACE_SIZE      0x100000

#define HOST_FW_VADDR  0x10000000
#define HOST_FW_SIZE   0xA0000
#define HOST_BIOS_VADDR 0x20000000
#define HOST_BIOS_SIZE  0x40000
#define HOST_SYS_MEM_VADDR 0x30000000
#define HOST_SYS_MEM_SIZE 0x100000

#define GUEST_FW_PADDR  0x0
#define GUEST_BIOS_PADDR 0xC0000
#define GUEST_SYS_MEM_PADDR 0x100000

#define DEFAULT_VM_STORE_DIR "/tmp/vms/"

#define NR_MAX_VCPUS 4
#define NR_MAX_IOAPIC_IRQS 32

// vcpu states
#define STATE_NOT_STARTED 0
#define STATE_RUNNING 1
#define STATE_HALTED 2

// Reasons we've broken out of RunVcpu
enum {
      VM_SHUTDOWN = 1,
      VM_HALTED = 2,
      VM_UNHANDLED_EXIT = 3
};

typedef struct hypervisor hv_t;

typedef struct segment {
  uint32_t selector;
  ulong base;
  uint32_t limit;
  uint32_t flags;
} segment_t;


typedef struct x86_cpu_regs {
  ulong gpr[CPU_NB_REGS];
  ulong eip;
  ulong eflags;

  /* cr0, cr3, cr4, etc */
  ulong control[5];
  int32_t a20_mask;

  /* segments */
  segment_t segment[6];
  segment_t ldt;
  segment_t tr;
  segment_t gdt;
  segment_t idt;

  uint8_t tpr;
  uint32_t apicbase;

  uint64_t efer;
} x86_cpu_regs_t;

typedef struct lapic {
  uint32_t isr;
} lapic_t;

struct interrupt_entry {
  uint8_t irq;
  struct interrupt_entry *next;
};

typedef struct ioapic {
  // max number of irqs this ioapic can handle
  uint32_t max_irqs;
  // redirect table specifying which CPU to notify for which IRQ
  // for the guest to program
  uint32_t irq_redir_table[NR_MAX_IOAPIC_IRQS];
  // eventfd used by devices to send IRQs
  int s[2];
  hv_t *hv;
  pthread_t ioapic_thread;
  pthread_mutex_t queue_lock;
  uint32_t nr_pending;
  struct interrupt_entry *interrupt_queue;
} ioapic_t;

typedef struct vcpu {
  uint32_t id;
  int driver_fd;
  hv_t *hv;
  bool dirty;
  uint32_t state;
  void *comm;
  pthread_mutex_t state_access_mutex;
  pthread_cond_t startcpu;
  x86_cpu_regs_t regs;
  pthread_mutex_t lapic_access_mutex;
  lapic_t lapic;
} vcpu_t;

// TODO: this needs to be hypervisor agnostic,
// the structure should not give clues that it is designed to be agnostic
typedef struct hypervisor {
  int fd;
  int vm_fd;
  int fw_memfd;
  int sys_memfd;

  // kvm specific
  int cur_slot;

  vcpu_t *vcpus[NR_MAX_VCPUS];

  // machine, vm specific things can go here because we have one VMM per VM
  size_t fw_size;
  void *fw;
  int fw_mem_id;

  size_t vga_size;
  void *vga;
  int vga_mem_id;

  size_t bios_rom_size;
  void *bios_rom;
  int bios_rom_mem_id;

  size_t sys_mem_size;
  void *sys_mem;
  int sys_mem_id;

  // APIC members
  pthread_mutex_t ioapic_access_mutex;
  ioapic_t *ioapic;

  // global lock for the device bus
  pthread_mutex_t bus_access_mutex;
} hv_t;

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
#endif
