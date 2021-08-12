#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include "vmm.h"
#include "devicebus.h"
#include "apic.h"

#define DEBUG 0

static void set_kvm_seg(struct kvm_segment *lhs, segment_t *rhs)
{
    unsigned flags = rhs->flags;
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->type = (flags >> DESC_TYPE_SHIFT) & 15;
    lhs->present = (flags & DESC_P_MASK) != 0;
    lhs->dpl = (flags >> DESC_DPL_SHIFT) & 3;
    lhs->db = (flags >> DESC_B_SHIFT) & 1;
    lhs->s = (flags & DESC_S_MASK) != 0;
    lhs->l = (flags >> DESC_L_SHIFT) & 1;
    lhs->g = (flags & DESC_G_MASK) != 0;
    lhs->avl = (flags & DESC_AVL_MASK) != 0;
    lhs->unusable = !lhs->present;
    lhs->padding = 0;
}

static void get_kvm_seg(segment_t *lhs, const struct kvm_segment *rhs)
{
    lhs->selector = rhs->selector;
    lhs->base = rhs->base;
    lhs->limit = rhs->limit;
    lhs->flags = (rhs->type << DESC_TYPE_SHIFT) |
                 ((rhs->present && !rhs->unusable) * DESC_P_MASK) |
                 (rhs->dpl << DESC_DPL_SHIFT) |
                 (rhs->db << DESC_B_SHIFT) |
                 (rhs->s * DESC_S_MASK) |
                 (rhs->l << DESC_L_SHIFT) |
                 (rhs->g * DESC_G_MASK) |
                 (rhs->avl * DESC_AVL_MASK);
}

static void print_dtable(const char *name, struct kvm_dtable *dtable)
{
    printf(" %s                 %016lx  %08hx\n",
           name, (uint64_t) dtable->base, (uint16_t) dtable->limit);
}


static void print_segment(const char *name, struct kvm_segment *seg)
{
    printf
        (" %s       %04hx      %016lx  %08x  %02hhx    %x %x   %x  %x %x %x %x\n",
         name, (uint16_t) seg->selector, (uint64_t) seg->base,
         (uint32_t) seg->limit, (uint8_t) seg->type, seg->present,
         seg->dpl, seg->db, seg->s, seg->l, seg->g, seg->avl);
}


static void dump_sregs(struct kvm_sregs *sregs)
{
    uint64_t cr0, cr2, cr3;
    uint64_t cr4, cr8;

    cr0 = sregs->cr0;
    cr2 = sregs->cr2;
    cr3 = sregs->cr3;
    cr4 = sregs->cr4;
    cr8 = sregs->cr8;

    printf(" cr0: %016lx   cr2: %016lx   cr3: %016lx\n", cr0, cr2, cr3);
    printf(" cr4: %016lx   cr8: %016lx\n", cr4, cr8);
    printf("\n Segment registers:\n");
    printf(" ------------------\n");
    printf(" register  selector  base              limit     type  p dpl db s l g avl\n");

    print_segment("cs ", &sregs->cs);
    print_segment("ss ", &sregs->ss);
    print_segment("ds ", &sregs->ds);
    print_segment("es ", &sregs->es);
    print_segment("fs ", &sregs->fs);
    print_segment("gs ", &sregs->gs);
    print_segment("tr ", &sregs->tr);
    print_segment("ldt", &sregs->ldt);
    print_dtable("gdt", &sregs->gdt);
    print_dtable("idt", &sregs->idt);
}

static void get_and_dump_sregs(int vcpufd)
{
    struct kvm_sregs sregs;
    int ret;

    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");

    dump_sregs(&sregs);
}



hv_t *hvInitHypervisor(void) {
  hv_t *hv = NULL;

  hv = malloc(sizeof(*hv));
  if (hv == NULL) {
    return NULL;
  }
  bzero(hv, sizeof(*hv));

  hv->ioapic = initIoApic(hv);
  if (!hv->ioapic) {
    goto err;
  }

  pthread_mutex_init(&hv->ioapic_access_mutex, NULL);
  pthread_mutex_init(&hv->bus_access_mutex, NULL);

  hv->fd = open("/dev/kvm", O_RDWR|O_CLOEXEC);
  if (hv->fd < 0) {
    goto err;
  }

  // there's only one instance of this process per VM, so just go ahead and
  // create a machine
  hv->vm_fd = ioctl(hv->fd, KVM_CREATE_VM, 0);
  if (hv->vm_fd < 0) {
    goto err;
  }

  return hv;

err:
  if (hv->fd)
    close(hv->fd);

  if (hv->ioapic)
    free(hv->ioapic);

  free(hv);

  return NULL;
}

int hvCreateInterruptController(hv_t *hv) {
  return ioctl(hv->vm_fd, KVM_CREATE_IRQCHIP, 0);
}

int hvCreateVcpu(hv_t *hv, vcpu_t *vcpu) {
  return ioctl(hv->vm_fd, KVM_CREATE_VCPU, vcpu->id);
}

int hvEstablishComm(vcpu_t *vcpu) {
  long mmap_size = ioctl(vcpu->hv->fd, KVM_GET_VCPU_MMAP_SIZE, 0);
  if (mmap_size < 0) {
    return mmap_size;
  }

  vcpu->comm = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED,
		    vcpu->driver_fd, 0);

  if (vcpu->comm == MAP_FAILED) {
    return -errno;
  }

  return 0;
}

int hvSetCpuid(vcpu_t *vcpu) {
  // only going to set ebx for now
  // TODO: Make sure this isn't exposing anything sensitive
  int err = 0;
  struct kvm_cpuid2 *cpuid = calloc(1, sizeof(struct kvm_cpuid2) + sizeof(struct kvm_cpuid_entry2)*0x1);
  uint32_t ebx = vcpu->id << 24;
  cpuid->entries[0].ebx = ebx;
  cpuid->nent = 1;
  err = ioctl(vcpu->driver_fd, KVM_SET_CPUID2, cpuid);
  if (err < 0)
    perror("KVM_SET_CPUID2");

  free(cpuid);
  return err;
}

int hvSetVcpuRegisters(vcpu_t *vcpu) {
  int ret = 0;
  struct kvm_regs r = {0};
  struct kvm_sregs sr = {0};

  r.rax = vcpu->regs.gpr[G_EAX];
  r.rbx = vcpu->regs.gpr[G_EBX];
  r.rcx = vcpu->regs.gpr[G_ECX];
  r.rdx = vcpu->regs.gpr[G_EDX];
  r.rsi = vcpu->regs.gpr[G_ESI];
  r.rdi = vcpu->regs.gpr[G_EDI];
  r.rsp = vcpu->regs.gpr[G_ESP];
  r.rbp = vcpu->regs.gpr[G_EBP];

  r.rflags = vcpu->regs.eflags;
  r.rip = vcpu->regs.eip;

  ret = ioctl(vcpu->driver_fd, KVM_SET_REGS, &r);
  if (ret < 0)
    return ret;

  set_kvm_seg(&sr.cs, &vcpu->regs.segment[S_CS]);
  set_kvm_seg(&sr.ds, &vcpu->regs.segment[S_DS]);
  set_kvm_seg(&sr.es, &vcpu->regs.segment[S_ES]);
  set_kvm_seg(&sr.fs, &vcpu->regs.segment[S_FS]);
  set_kvm_seg(&sr.gs, &vcpu->regs.segment[S_GS]);
  set_kvm_seg(&sr.ss, &vcpu->regs.segment[S_SS]);

  set_kvm_seg(&sr.tr, &vcpu->regs.tr);
  set_kvm_seg(&sr.ldt, &vcpu->regs.ldt);

  sr.idt.limit = vcpu->regs.idt.limit;
  sr.idt.base = vcpu->regs.idt.base;
  memset(sr.idt.padding, 0, sizeof(sr.idt.padding));

  sr.gdt.limit = vcpu->regs.gdt.limit;
  sr.gdt.base = vcpu->regs.gdt.base;
  memset(sr.gdt.padding, 0, sizeof(sr.gdt.padding));

  sr.cr0 = vcpu->regs.control[0];
  sr.cr2 = vcpu->regs.control[2];
  sr.cr3 = vcpu->regs.control[3];
  sr.cr4 = vcpu->regs.control[4];

  // cr8 = apic.tpr[7:4]
  sr.cr8 = vcpu->regs.tpr >> 4;
  sr.apic_base = vcpu->regs.apicbase;

  sr.efer = vcpu->regs.efer;

  memset(sr.interrupt_bitmap, 0, sizeof(sr.interrupt_bitmap));

  ret = ioctl(vcpu->driver_fd, KVM_SET_SREGS, &sr);
  if (ret < 0)
    return ret;

  return ret;
}

int hvGetVcpuRegisters(vcpu_t *vcpu) {
  int ret;
  struct kvm_regs r = {0};
  struct kvm_sregs sr = {0};

  ret = ioctl(vcpu->driver_fd, KVM_GET_REGS, &r);
  if (ret < 0)
    return ret;

  vcpu->regs.gpr[G_EAX] = r.rax;
  vcpu->regs.gpr[G_EBX] = r.rbx;
  vcpu->regs.gpr[G_ECX] = r.rcx;
  vcpu->regs.gpr[G_EDX] = r.rdx;
  vcpu->regs.gpr[G_ESI] = r.rsi;
  vcpu->regs.gpr[G_EDI] = r.rdi;
  vcpu->regs.gpr[G_ESP] = r.rsp;
  vcpu->regs.gpr[G_EBP] = r.rbp;

  vcpu->regs.eflags = r.rflags;
  vcpu->regs.eip = r.rip;

  ret = ioctl(vcpu->driver_fd, KVM_GET_SREGS, sr);
  if (ret < 0)
    return ret;

  get_kvm_seg(&vcpu->regs.segment[S_CS], &sr.cs);
  get_kvm_seg(&vcpu->regs.segment[S_DS], &sr.ds);
  get_kvm_seg(&vcpu->regs.segment[S_ES], &sr.es);
  get_kvm_seg(&vcpu->regs.segment[S_FS], &sr.fs);
  get_kvm_seg(&vcpu->regs.segment[S_GS], &sr.gs);
  get_kvm_seg(&vcpu->regs.segment[S_SS], &sr.ss);

  get_kvm_seg(&vcpu->regs.tr, &sr.tr);
  get_kvm_seg(&vcpu->regs.ldt, &sr.ldt);

  vcpu->regs.idt.limit = sr.idt.limit;
  vcpu->regs.idt.base = sr.idt.base;

  vcpu->regs.gdt.limit = sr.gdt.limit;
  vcpu->regs.gdt.base = sr.gdt.base;

  vcpu->regs.control[0] = sr.cr0;
  vcpu->regs.control[2] = sr.cr2;
  vcpu->regs.control[3] = sr.cr3;
  vcpu->regs.control[4] = sr.cr4;

  vcpu->regs.tpr = sr.cr8 << 4;
  vcpu->regs.apicbase = sr.apic_base;

  vcpu->regs.efer = sr.efer;

  return ret;
}

/* Setup a guest memory region, return the slot id used */

int hvSetMemory(hv_t *hv, void *hva, size_t len, uint64_t gpa, bool readonly) {
  int slot;
  struct kvm_userspace_memory_region mem = {0};

  slot = hv->cur_slot++;
  mem.slot = slot;
  mem.guest_phys_addr = gpa;
  mem.userspace_addr = (uint64_t) hva;
  mem.flags = readonly ? KVM_MEM_READONLY : 0;
  mem.memory_size = len;

  if (ioctl(hv->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem) < 0)
    return -1;

  return slot;
}

void hvDelMemory(hv_t *hv, int slot) {
  struct kvm_userspace_memory_region mem = {0};

  // To delete the backing of a memory slot, we just reduce size to 0
  mem.slot = slot;
  mem.memory_size = 0;

  // TODO: Can this fail?
  ioctl(hv->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem);
}

int hvRunVcpu(vcpu_t *vcpu) {
  int ret = 0;
  int run_ret = 0;
  struct kvm_run *run = vcpu->comm;

  do {

    if (vcpu->dirty) {
      hvSetVcpuRegisters(vcpu);
      vcpu->dirty = false;
    }

    // check if there are interrupts that need injecting
    checkAndSendInterrupt(vcpu->hv, vcpu);

    run_ret = ioctl(vcpu->driver_fd, KVM_RUN, 0);

    // update our internal vcpu state after the vmexit
    hvGetVcpuRegisters(vcpu);

    if (run_ret < 0) {
      if (run_ret == -EINTR || run_ret == -EAGAIN) {
        fprintf(stderr, "Received externl interrupt while executing Vcpu\n");
      }
    }

    // platform agnostic 'direction'
    uint8_t direction = 0;
    switch (run->exit_reason) {
    case KVM_EXIT_HLT:
      if (DEBUG) {
        struct kvm_regs regs;
        ioctl(vcpu->driver_fd, KVM_GET_REGS, &regs);
        printf("vcpu %d halted at eip: 0x%llx\n", vcpu->id, regs.rip);
      }
      vcpu->state = STATE_HALTED;
      ret = waitForSipi(vcpu);
      break;
    case KVM_EXIT_SHUTDOWN:
      get_and_dump_sregs(vcpu->driver_fd);
      ret = VM_SHUTDOWN;
      break;
    case KVM_EXIT_IO:
      /* handle io here */
      if (run->io.direction == KVM_EXIT_IO_IN) {
        direction = IO_DIRECTION_IN;
      } else if (run->io.direction == KVM_EXIT_IO_OUT) {
        direction = IO_DIRECTION_OUT;
      } else {
        ret = VM_UNHANDLED_EXIT;
        break;
      }

      ret = dbusHandlePioAccess(vcpu,
                                run->io.port,
                                (uint8_t *)run + run->io.data_offset,
                                direction,
                                run->io.size,
                                run->io.count);
      break;
    case KVM_EXIT_MMIO:
      /* handle mmio here */
      ret = dbusHandleMmioAccess(vcpu,
                                run->mmio.phys_addr,
                                (uint64_t *)run->mmio.data,
                                run->mmio.len,
                                run->mmio.is_write);
      break;
    case KVM_EXIT_IRQ_WINDOW_OPEN:
      ret = 0;
      if (DEBUG)
        printf("WINDOW OPEN\n");
      break;
    default:
      if (DEBUG) {
        printf("exit code: %d\n", run->exit_reason);
      }
      ret = VM_UNHANDLED_EXIT;
      break;
    }
  } while(!ret);

  return ret;
}
