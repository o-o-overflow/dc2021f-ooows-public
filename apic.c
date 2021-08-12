#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include "vmm.h"
#include "apic.h"
#include "iostructs.h"

#define DEBUG 0

extern hv_t *g_hv;
extern uint32_t g_nvcpus;

// #####################
// # GENERIC APIC CODE #
// #####################
bool apicAccess(vcpu_t *vcpu, uint64_t phys_addr) {
  return lapicAccess(vcpu, phys_addr) || ioApicAccess(phys_addr);
}

int apicMmio(vcpu_t *vcpu,
    uint64_t phys_addr,
    uint64_t *data,
    uint32_t len,
    uint8_t is_write) {
  int value = 0;

  if (len != 4)
    return 0;

  if (lapicAccess(vcpu, phys_addr)) {
    value = lapicMmio(vcpu, phys_addr, data,
              len, is_write);
  }
  else if (ioApicAccess(phys_addr)) {
    value = ioApicMmio(vcpu, phys_addr, data,
              len, is_write);
  }

  if (!is_write) {
    *((uint32_t *)data) = value;
  }

  return 0;
}

// ##############
// # LAPIC CODE #
// ##############
bool lapicAccess(vcpu_t *vcpu, uint64_t phys_addr) {
  uint64_t apicbase = vcpu->regs.apicbase & MSR_IA32_APICBASE_BASE;
  return phys_addr >= apicbase && phys_addr < apicbase + PAGE_SIZE;
}

void lapicDoSipi(vcpu_t *vcpu, uint32_t vector) {
  hv_t *hv = vcpu->hv;

  unsigned cpuid = vector & 0x3;
  vcpu_t *target = hv->vcpus[cpuid];

  // ensure cpu exists
  if (!target)
    return;

  // refuse to start a CPU that is already running
  if (target->state == STATE_RUNNING)
    return;

  if (DEBUG)
    printf("Sending SIPI to %d with vector 0x%x\n", cpuid, vector & ~0x3);


  pthread_mutex_lock(&target->state_access_mutex);

  target->state = STATE_RUNNING;
  target->regs.eip = vector & ~0x3;
  target->dirty = true;

  pthread_cond_signal(&target->startcpu);

  pthread_mutex_unlock(&target->state_access_mutex);
}

int lapicMmioWrite(vcpu_t *vcpu,
                   uint64_t phys_addr,
                   uint64_t *data,
                   uint32_t len) {

  unsigned offset = phys_addr & 0xfff;
  uint32_t vector = *(uint32_t *)data;

  switch (offset) {
  case LAPIC_OFF_IPI:
    lapicDoSipi(vcpu, vector);
    break;

  case LAPIC_OFF_EOI:
    pthread_mutex_lock(&vcpu->lapic_access_mutex);
    // turn off the bit in the "in service register"
    vcpu->lapic.isr &= ~(1<<(vector&0x1f));
    pthread_mutex_unlock(&vcpu->lapic_access_mutex);
    break;

    // don't allow writes to any others for the time being
  }

  return 0;
}

int lapicMmioRead(vcpu_t *vcpu,
                  uint64_t phys_addr,
                  uint64_t *data,
                  uint32_t len) {

  unsigned offset = phys_addr & 0xfff;

  switch (offset) {
    case LAPIC_OFF_ID:
      return vcpu->id;
      break;
    default:
      break;
  }

  return 0;
}

int lapicMmio(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data,
              uint32_t len, uint8_t is_write) {
  int ret = 0;

  // never allow a read or write which is not a dword
  if (len != sizeof(int)) {
    return ret;
  }

  if (is_write) {
    ret = lapicMmioWrite(vcpu, phys_addr, data, len);
  } else {
    ret = lapicMmioRead(vcpu, phys_addr, data, len);
  }

  return ret;
}


// ###############
// # IOAPIC CODE #
// ###############
bool ioApicAccess(uint64_t phys_addr) {
  uint64_t end_valid_addr = PADDR_IOAPIC +\
    sizeof(uint32_t)*(NR_MAX_IOAPIC_IRQS-1);
  return (phys_addr >= PADDR_IOAPIC && phys_addr <= end_valid_addr);
}

ioapic_t * initIoApic(hv_t *hv) {
  ioapic_t *ioapic = calloc(1, sizeof(ioapic_t));
  if (!ioapic) {
    goto err;
  }

  ioapic->hv = hv;
  ioapic->max_irqs = NR_MAX_IOAPIC_IRQS;
  ioapic->interrupt_queue = NULL;
  pthread_mutex_init(&ioapic->queue_lock, NULL);
  if (socketpair(AF_UNIX, SOCK_DGRAM, 0, ioapic->s) < 0) {
    perror("socketpair failed in initIoApic\n");
    goto err;
  }

  return ioapic;

err:
  if (ioapic)
    free(ioapic);
  return NULL;
}

bool redtblAccess(uint64_t phys_addr) {
  uint64_t redir_table_start = PADDR_IOAPIC + IOAPIC_OFF_REDTBL;
  uint64_t redit_table_end_valid = redir_table_start
    + sizeof(uint32_t)*(NR_MAX_IOAPIC_IRQS-1);
  return (phys_addr >= redir_table_start && phys_addr <= redit_table_end_valid);
}

int redtblIdx(uint64_t phys_addr) {
  uint64_t offset = phys_addr - PADDR_IOAPIC - IOAPIC_OFF_REDTBL;
  int idx = offset / sizeof(uint32_t);
  assert(idx < NR_MAX_IOAPIC_IRQS);
  return idx;
}

int ioApicMmioWrite(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data) {

  int ret = 0;
  if (redtblAccess(phys_addr)) {
    int idx = redtblIdx(phys_addr);
    uint32_t entry = (uint32_t)*data;
    g_hv->ioapic->irq_redir_table[idx] = entry;
  }

  return ret;
}

int ioApicMmioRead(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data) {

  int ret = 0;
  uint64_t offset = phys_addr - PADDR_IOAPIC;

  if (redtblAccess(phys_addr)) {
    int idx = redtblIdx(phys_addr);
    ret = g_hv->ioapic->irq_redir_table[idx];
    goto end_read;
  }

  switch(offset) {
    case IOAPIC_OFF_ID:
      // "OOO"
      ret = 0x4f4f4f;
      break;

    case IOAPIC_OFF_MAX_IRQS:
      ret = NR_MAX_IOAPIC_IRQS;
      break;
  }

end_read:
  return ret;
}


int ioApicMmio(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data,
              uint32_t len, uint8_t is_write) {
  // only service 4 byte reads/writes
  if (len != 4)
    return 0;
  pthread_mutex_lock(&g_hv->ioapic_access_mutex);

  int ret = 0;
  if (is_write) {
    ret = ioApicMmioWrite(vcpu, phys_addr, data);
  }
  else {
    ret = ioApicMmioRead(vcpu, phys_addr, data);
  }

  pthread_mutex_unlock(&g_hv->ioapic_access_mutex);
  return ret;
}

int ioApicSendInterrupt(hv_t *hv, uint8_t irq) {
  if(DEBUG) {
    printf("Being asked to send irq %d\n", irq);
  }
  int ret = 0;
  // TODO: fine? should be since we only support 32 IRQS
  // May want to make it more clear elsewhere though
  irq = irq & 0x1f;
  // lock access to the ioapic so we can read the redir table
  pthread_mutex_lock(&hv->ioapic_access_mutex);
  union redirTableEntry entry;
  entry.val= hv->ioapic->irq_redir_table[irq];
  uint16_t dest_vcpu_id = entry.fields.dest_cpu;
  // ensure the dest cpu exists
  if (dest_vcpu_id >= g_nvcpus) {
    ret = -1;
    goto end_handle_intrrupt2;
  }

  vcpu_t *dest_vcpu = hv->vcpus[dest_vcpu_id];

  // check if vcpu exists
  if (!dest_vcpu) {
    ret = -1;
    goto end_handle_intrrupt2;
  }

  // if cpu has never been started, we won't wake it with a device irq
  if (dest_vcpu->state == STATE_NOT_STARTED) {
    ret = -1;
    goto end_handle_intrrupt2;
  }

  // if it's currently halted, we'll wake it
  if (dest_vcpu->state == STATE_HALTED) {
    if (DEBUG)
      printf("Going to wake vcpu %d for a device irq\n", dest_vcpu_id);
    pthread_mutex_lock(&dest_vcpu->state_access_mutex);
    dest_vcpu->state = STATE_RUNNING;
    pthread_cond_signal(&dest_vcpu->startcpu);
    pthread_mutex_unlock(&dest_vcpu->state_access_mutex);
  }

  // if it exists, take the lapic lock for that vcpu
  // so we can check the isr
  pthread_mutex_lock(&dest_vcpu->lapic_access_mutex);

  // we're still waiting on an interrupt from earlier to be serviced
  if (dest_vcpu->lapic.isr & (1 << irq)) {
    ret = -1;
    if (DEBUG) {
      printf("vcpu %d is still handling an irq of %d -- isr: 0x%x\n",
          dest_vcpu_id, irq, dest_vcpu->lapic.isr);
    }
    goto end_handle_intrrupt1;
  }

  if (DEBUG) {
    printf("sending irq 0x%x (vector %d) to cpu %d\n",
        irq, entry.fields.vector, dest_vcpu_id);
  }

  // TODO(ctf) - make sure it's okay for this vector to go unchecked
  // otherwise we're good to send the interrupt
  // KVM takes the vector, not the IRQ, so we need to retrieve
  // that from the redir table
  struct kvm_interrupt kvm_i;
  kvm_i.irq = (uint32_t)entry.fields.vector;
  int err = ioctl(dest_vcpu->driver_fd, KVM_INTERRUPT, &kvm_i);
  if (err < 0) {
    perror("KVM_INTERRUPT");
    ret = -1;
    goto end_handle_intrrupt1;
  }
  if (DEBUG)
    printf("\n\nINJECTED INTERRUPT\n\n");

  // finally set the isr bit on the dest vcpu's lapic
  dest_vcpu->lapic.isr |= (1 << irq);

end_handle_intrrupt1:
  pthread_mutex_unlock(&dest_vcpu->lapic_access_mutex);
end_handle_intrrupt2:
  pthread_mutex_unlock(&hv->ioapic_access_mutex);
  if(DEBUG) {
    printf("Returning from trying to send irq %d\n", irq);
  }
  return ret;
}

void * ioApicThread(void *arg) {
  hv_t *hv = (hv_t *)arg;
  int sock = hv->ioapic->s[0];
  int ret = 0;
  uint8_t irq;

  while(read(sock, &irq, sizeof(irq)) == sizeof(irq)) {
    if (DEBUG)
      printf("ioapic received irq: 0x%x\n", irq);
    //ret = ioApicSendInterrupt(hv, irq);
    ret = queueInterrupt(hv->ioapic, irq);
    if ( (ret < 0) && DEBUG) {
      printf("Didn't queue irq %d\n", irq);
    }
  }
  return NULL;
}

int queueInterrupt(ioapic_t *ioapic, uint8_t irq) {
  int ret = 0;

  pthread_mutex_lock(&ioapic->queue_lock);

  if (ioapic->nr_pending >= QUEUE_MAX_INTERRUPTS) {
    ret = -1;
    goto end;
  }

  struct interrupt_entry *entry = calloc(1, sizeof(struct interrupt_entry));
  if (!entry) {
    ret = -1;
    goto end;
  }

  entry->irq = irq;

  if (!ioapic->interrupt_queue) {
    ioapic->interrupt_queue = entry;
    goto end;
  }

  struct interrupt_entry *cur = ioapic->interrupt_queue;
  while (cur->next) {
    cur = cur->next;
  }
  cur->next = entry;

end:
  if (!ret)
    ioapic->nr_pending++;
  pthread_mutex_unlock(&ioapic->queue_lock);
  return ret;
}

struct interrupt_entry * dequeueInterrupt(ioapic_t *ioapic) {
  struct interrupt_entry *ret = NULL;
  int err = 0;
  pthread_mutex_lock(&ioapic->queue_lock);
  if (ioapic->nr_pending == 0) {
    err = -1;
    goto end;
  }

  if (!ioapic->interrupt_queue) {
    err = -1;
    goto end;
  }

  ret = ioapic->interrupt_queue;
  ioapic->interrupt_queue = ret->next;

end:
  if (!err)
    ioapic->nr_pending--;
  pthread_mutex_unlock(&ioapic->queue_lock);
  return ret;
}

bool haveInterrupts(ioapic_t *ioapic) {
  pthread_mutex_lock(&ioapic->queue_lock);
  bool ret = (ioapic->nr_pending != 0);
  pthread_mutex_unlock(&ioapic->queue_lock);
  return ret;
}


int checkAndSendInterrupt(hv_t *hv, vcpu_t *vcpu) {
  // do we have any interrupts queued?
  if (haveInterrupts(hv->ioapic)) {

    // Can we send an interrupt?
    if ( ((struct kvm_run *)vcpu->comm)->ready_for_interrupt_injection) {
      struct interrupt_entry *entry = dequeueInterrupt(hv->ioapic);
      if (!entry)
        return -1;
      ioApicSendInterrupt(hv, entry->irq);
      free(entry);
    }

    // Can't send and have queued interrupts. send a request for a window
    else {
      ((struct kvm_run *)vcpu->comm)->request_interrupt_window = 1;
    }
  }

  else {
    // If the window is open and we have no pending interrupts
    // turn off the request
    if ( ((struct kvm_run *)vcpu->comm)->ready_for_interrupt_injection) {
      ((struct kvm_run *)vcpu->comm)->request_interrupt_window = 0;
    }
  }

  return 0;
}
