#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "vmm.h"
#include "devicebus.h"
#include "apic.h"
#define DEBUG 0

hv_t *g_hv = NULL;
uint32_t g_nvcpus = 0;

// initialize vCPU to x86 reset state
void x86CpuReset(vcpu_t *vcpu, bool bsp) {
  x86_cpu_regs_t *regs = &vcpu->regs;

  bzero(regs, sizeof(*regs));

  regs->a20_mask = ~0;

  // Cache Disable
  // Extension Type
  regs->control[0] = 0x60000010;

  regs->idt.limit = 0xffff;
  regs->gdt.limit = 0xffff;
  regs->ldt.limit = 0xffff;
  regs->ldt.flags = DESC_P_MASK | (2 << DESC_TYPE_SHIFT);
  regs->tr.limit  = 0xffff;
  regs->tr.flags  = DESC_P_MASK | (11 << DESC_TYPE_SHIFT);

  regs->segment[S_CS].selector = 0xf000;
  regs->segment[S_CS].base     = 0xf0000;
  regs->segment[S_CS].limit    = 0xffff;
  regs->segment[S_CS].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_CS_MASK | DESC_R_MASK | DESC_A_MASK;

  regs->segment[S_DS].selector = 0x0;
  regs->segment[S_DS].base     = 0x0;
  regs->segment[S_DS].limit    = 0xffff;
  regs->segment[S_DS].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_W_MASK | DESC_A_MASK;

  regs->segment[S_ES].selector = 0x0;
  regs->segment[S_ES].base     = 0x0;
  regs->segment[S_ES].limit    = 0xffff;
  regs->segment[S_ES].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_W_MASK | DESC_A_MASK;

  regs->segment[S_SS].selector = 0x0;
  regs->segment[S_SS].base     = 0x0;
  regs->segment[S_SS].limit    = 0xffff;
  regs->segment[S_SS].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_W_MASK | DESC_A_MASK;

  regs->segment[S_FS].selector = 0x0;
  regs->segment[S_FS].base     = 0x0;
  regs->segment[S_FS].limit    = 0xffff;
  regs->segment[S_FS].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_W_MASK | DESC_A_MASK;

  regs->segment[S_GS].selector = 0x0;
  regs->segment[S_GS].base     = 0x0;
  regs->segment[S_GS].limit    = 0xffff;
  regs->segment[S_GS].flags    = DESC_P_MASK |\
    DESC_S_MASK | DESC_W_MASK | DESC_A_MASK;

  regs->eip = 0xfff0;
  regs->gpr[G_EDX] = 0x663; // cpuid version
  regs->eflags = 0x2;

  regs->tpr = 0;

  regs->apicbase = APIC_DEFAULT_ADDRESS | MSR_IA32_APICBASE_ENABLE;

  if (bsp)
    regs->apicbase |= MSR_IA32_APICBASE_BSP;
}

// thread func that runs CPU in a loop until halt
static int runVcpu(vcpu_t *vcpu) {
  return hvRunVcpu(vcpu);
}

static int setupGuestMemory(void) {
  int ret = 0;
  /* our goal is to roughly create memory similar to the i440fx motherboard
   * chipset */

  /*
	0x00000000	0x000003FF	1 KiB	Real Mode IVT (Interrupt Vector Table)	unusable in real mode	640 KiB RAM ("Low memory")
	0x00000400	0x000004FF	256 bytes	BDA (BIOS data area)
	0x00000500	0x00007BFF	almost 30 KiB	Conventional memory	usable memory
	0x00007C00	0x00007DFF	512 bytes	Your OS BootSector
	0x00007E00	0x0007FFFF	480.5 KiB	Conventional memory
	0x00080000	0x0009FFFF	128 KiB	EBDA (Extended BIOS Data Area)	partially used by the EBDA
	0x000A0000	0x000BFFFF	128 KiB	Video display memory	hardware mapped	384 KiB System / Reserved ("Upper Memory")
	0x000C0000	0x000C7FFF	32 KiB (typically)	Video BIOS	ROM and hardware mapped / Shadow RAM
	0x000C8000	0x000EFFFF	160 KiB (typically)	BIOS Expansions
	0x000F0000	0x000FFFFF	64 KiB	Motherboard BIOS
   */

  ret = memfd_create("vmram", 0);
  if (ret < 0) {
    return ret;
  }

  g_hv->fw_memfd = ret;

  ret = ftruncate(g_hv->fw_memfd, HOST_FW_SIZE);
  if (ret < 0) {
    goto cleanup;
  }

  /* map in main ram, goes up to video memory */
  g_hv->fw_size = HOST_FW_SIZE;
  g_hv->fw = mmap((void *)HOST_FW_VADDR,
                   HOST_FW_SIZE,
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED|MAP_FIXED,
                   g_hv->fw_memfd,
                   0);
  if (g_hv->fw == MAP_FAILED) {
    ret = -1;
    goto cleanup;
  }

  ret = hvSetMemory(g_hv, g_hv->fw, g_hv->fw_size, GUEST_FW_PADDR, false);
  if (ret < 0)
    goto cleanup;
  g_hv->fw_mem_id = ret;

  ret = memfd_create("vmsysmem", 0);
  if (ret < 0) {
    return ret;
  }

  g_hv->sys_memfd = ret;

  ret = ftruncate(g_hv->sys_memfd, HOST_SYS_MEM_SIZE);
  if (ret < 0) {
    goto cleanup;
  }
  /* map in Kernel mem. TODO: command line. for now it's 1MB starting at 1MB*/
  g_hv->sys_mem_size = HOST_SYS_MEM_SIZE;
  g_hv->sys_mem = mmap((void *)HOST_SYS_MEM_VADDR,
           HOST_SYS_MEM_SIZE,
           PROT_READ|PROT_WRITE,
           MAP_SHARED|MAP_FIXED,
           g_hv->sys_memfd,
           0);
  if (g_hv->sys_mem == MAP_FAILED) {
    ret = -1;
    printf("Map failed\n");
    goto cleanup;
  }

  ret = hvSetMemory(g_hv,
            g_hv->sys_mem,
            g_hv->sys_mem_size,
            GUEST_SYS_MEM_PADDR,
            false);
  if (ret < 0) {
    printf("hvSetMem failed\n");
    goto cleanup;
  }
  g_hv->sys_mem_id = ret;

  /* now BIOS, the VCPU will jump to 0xFFFF0 when it boots */
  g_hv->bios_rom_size = HOST_BIOS_SIZE;
  g_hv->bios_rom = mmap((void *)HOST_BIOS_VADDR,
                   HOST_BIOS_SIZE,
                   PROT_READ|PROT_WRITE,
                   MAP_ANON|MAP_SHARED|MAP_FIXED,
                   -1,
                   0);
  if (g_hv->bios_rom == MAP_FAILED) {
    ret = -1;
    goto cleanup;
  }

  // TODO: we might need to dynamically resolve the filesystem location of the
  // bios blob
  int fd = open("bios/bios", O_RDONLY);
  if (fd < 0) {
    ret = -1;
    goto cleanup;
  }

  ret = read(fd, g_hv->bios_rom + 0x3c000, 0x4000);
  if (ret < 0) {
    ret = -1;
    goto cleanup;
  }
  close(fd);

  ret = hvSetMemory(g_hv,
                    g_hv->bios_rom,
                    g_hv->bios_rom_size,
                    GUEST_BIOS_PADDR,
                    true);
  if (ret < 0)
    goto cleanup;

  g_hv->bios_rom_mem_id = ret;

  return 0;

 cleanup:
  if (g_hv->fw_memfd) {
    close(g_hv->fw_memfd);
  }

  if (g_hv->sys_memfd) {
    close(g_hv->sys_memfd);
  }

  if (g_hv->fw_mem_id) {
    hvDelMemory(g_hv, g_hv->fw_mem_id);
    g_hv->fw_mem_id = 0;
  }

  if (g_hv->vga_mem_id) {
    hvDelMemory(g_hv, g_hv->vga_mem_id);
    g_hv->vga_mem_id = 0;
  }

  if (g_hv->sys_mem_id) {
    hvDelMemory(g_hv, g_hv->sys_mem_id);
    g_hv->sys_mem_id = 0;
  }

  if (g_hv->bios_rom_mem_id) {
    hvDelMemory(g_hv, g_hv->bios_rom_mem_id);
    g_hv->bios_rom_mem_id = 0;
  }

  if (g_hv->fw) {
    munmap(g_hv->fw, g_hv->fw_size);
  }

  if (g_hv->vga) {
    munmap(g_hv->vga, g_hv->vga_size);
  }

  if (g_hv->bios_rom) {
    munmap(g_hv->bios_rom, g_hv->bios_rom_size);
  }

  if (g_hv->sys_mem) {
    munmap(g_hv->sys_mem, g_hv->sys_mem_size);
  }

  return ret;
}

static int initVcpu(vcpu_t *vcpu, bool bsp) {

  // ask the hypervisor for a new vcpu
  int ret = hvCreateVcpu(g_hv, vcpu);
  if (ret < 0) {
    return ret;
  }

  vcpu->driver_fd = ret;
  vcpu->hv = g_hv;
  vcpu->dirty = true;

  // establish sync primitives
  // TODO: ensure default mutex attributes are sane
  pthread_mutex_init(&vcpu->state_access_mutex, NULL);
  pthread_cond_init(&vcpu->startcpu, NULL);
  // init apic access mutex
  pthread_mutex_init(&vcpu->lapic_access_mutex, NULL);

  // only the BSP begins running
  if (bsp) {
    vcpu->state = STATE_RUNNING;
  }
  else {
    vcpu->state = STATE_NOT_STARTED;
  }

  // establish a channel so we can get notifications for the vcpu
  ret = hvEstablishComm(vcpu);
  if (ret < 0) {
    return ret;
  }

  x86CpuReset(vcpu, bsp);

  ret = hvSetVcpuRegisters(vcpu);
  if (ret < 0) {
    return ret;
  }

  ret = hvSetCpuid(vcpu);
  if (ret < 0) {
    return ret;
  }

  return ret;
}

static vcpu_t *createVcpu(void) {
  vcpu_t *vcpu = NULL;

  vcpu = malloc(sizeof(*vcpu));
  if (vcpu == NULL) {
    return NULL;
  }

  bzero(vcpu, sizeof(*vcpu));
  vcpu->id = g_nvcpus++;
  g_hv->vcpus[vcpu->id] = vcpu;

  return vcpu;
}

int waitForSipi(vcpu_t *vcpu) {
  hv_t *hv = vcpu->hv;
  // if all are halted, we've shut down
  bool all_halted = true;
  int i;
  // take the shutdown lock so none can change on us during check
  for (i=0; i < g_nvcpus; i++) {
    if (hv->vcpus[i]->state == STATE_RUNNING)
      all_halted = false;
  }
  if (all_halted) {
    printf("All vcpus are halted. Shutting down\n");
    return VM_SHUTDOWN;
  }

  // otherwise we can still hope for a wakeup
  pthread_mutex_lock(&vcpu->state_access_mutex);
  while (vcpu->state != STATE_RUNNING)
    pthread_cond_wait(&vcpu->startcpu, &vcpu->state_access_mutex);
  pthread_mutex_unlock(&vcpu->state_access_mutex);
  if (DEBUG)
    printf("VCPU %d woke up!\n", vcpu->id);
  return 0;
}


static void *vcpuThread(void *arg) {
  vcpu_t *vcpu = (vcpu_t *)arg;

  if (vcpu->state == STATE_NOT_STARTED)
    waitForSipi(vcpu);

  switch (runVcpu(vcpu)) {
  case VM_SHUTDOWN:
    printf("VM shutdown gracefully\n");
    break;
  case VM_HALTED:
    printf("VM executed hlt\n");
    break;
  case VM_UNHANDLED_EXIT:
    printf("VM attempted some unhandled case\n");
    break;
  default:
    printf("Unexpected VM exit, device emulation may have failed\n");
    break;
  }

  return NULL;
}

static int startVcpuThread(bool bsp, pthread_t *thread) {
    vcpu_t *v = createVcpu();
    if (v == NULL) {
      perror("Failed to create demo vcpu");
      return -1;
    }

    if (initVcpu(v, bsp) < 0) {
      perror("Failed to initialize vcpu");
      return -1;
    }

    if (pthread_create(thread, NULL, vcpuThread, v) < 0) {
      perror("Failed to create vCPU thread");
      return -1;
    }

    return 0;
}

void handleSigterm(int sig) {
  dbusTeardown();
  exit(0);
}

int initVmStore(char *vmname) {
  bool needsEnv = false;
  char *vmStoreDirPath = NULL;

  vmStoreDirPath = getenv("OOOWS_VM_STORE_DIR");
  if (vmStoreDirPath == NULL) {
    vmStoreDirPath = DEFAULT_VM_STORE_DIR;
    needsEnv = true;
  }

  if (access(vmStoreDirPath, O_RDONLY) < 0) {
    if (mkdir(vmStoreDirPath, 0700) < 0) {
      perror("Failed to make VM runtime store directory");
      return -1;
    }
  }

  int dirfd = open(vmStoreDirPath, O_DIRECTORY);
  if (dirfd < 0) {
    perror("Failed to open VM runtime store");
    return -1;
  }

  if (faccessat(dirfd, vmname, O_RDONLY, 0) < 0) {
    if (mkdirat(dirfd, vmname, 0700) < 0) {
      perror("Failed to make runtime store for requested VM");
      return -1;
    }
  }

  close(dirfd);

  if (needsEnv) {
    if (setenv("OOOWS_VM_STORE_DIR", vmStoreDirPath, 0)) {
      perror("Failed to set env for VM runtime store");
      return 1;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  int nvcpus = 1;

  if (argc < 3) {
    fprintf(stderr, "usage: %s <vmname> <virtdisk> [vcpus]\n", argv[0]);
    return 1;
  }

  if (setenv("OOOWS_VM_NAME", argv[1], 0)) {
    perror("Failed to set vm name env var");
    return 1;
  }

  if (setenv("OOOWS_VM_VIRTDISK", argv[2], 0)) {
    perror("Failed to set virtdisk env var");
    return 1;
  }

  if (initVmStore(argv[1]) < 0) {
    perror("Failed to initialize VM runtime storage");
    return 1;
  }

  if (argc > 3) {
    nvcpus = atoi(argv[3]);
    if (nvcpus > NR_MAX_VCPUS) {
      fprintf(stderr,
              "Invalid number of vCPUs specified, max is %d\n", NR_MAX_VCPUS);
    }
  }

  char *devconfigPath = "devices.config";
  if (argc > 4) {
    devconfigPath = argv[4];
  }

  if (dbusConfigFromFile(devconfigPath)) {
    perror("Failed to instantiate virtual hardware layout");
    return 1;
  }

  if (atexit(dbusTeardown)) {
    perror("Failed to install teardown logic");
    return 1;
  }

  signal(SIGTERM, handleSigterm);

  g_hv = hvInitHypervisor();
  if (g_hv == NULL) {
    perror("Failed to initialize hypervisor");
    return 1;
  }

  // spin up the ioapic thread
  if (pthread_create(&g_hv->ioapic->ioapic_thread, NULL, ioApicThread, g_hv) < 0) {
    printf("Failed to start ioapic thread\n");
    return 1;
  }

  /* if (hvCreateInterruptController(g_hv) < 0) { */
  /*   perror("Failed to setup interrupt controller"); */
  /*   return 1; */
  /* } */

  if (setupGuestMemory() < 0) {
    perror("Failed to setup guest memory");
    return 1;
  }

  pthread_t *threads = calloc(nvcpus, sizeof(pthread_t));
  if (threads == NULL) {
    perror("Failed to allocate space for vCPU threads");
    return 1;
  }
  bzero(threads, nvcpus * sizeof(pthread_t));

  int i=0;
  for (i=0;i<nvcpus;i++) {
    if (startVcpuThread(i == 0, &threads[i]) < 0) {
      goto cancel;
    }
  }

  int retval = 0;
  void *retp = &retval;
  for (i=0;i<nvcpus;i++) {
    pthread_join(threads[i], &retp);
  }

  return 0;

 cancel:
  for (i=0;i<nvcpus;i++) {
    if (threads[i]) {
      pthread_cancel(threads[i]);
    }
  }

  return 1;
}
