#ifndef APIC_H_
#define APIC_H_

#include "vmm.h"

#define PAGE_SIZE 0x1000

// lapic offsets
#define LAPIC_OFF_ID  0
#define LAPIC_OFF_IPI  4
#define LAPIC_OFF_EOI  8
#define LAPIC_OFF_ISR1 12

// ioapic offsets
#define IOAPIC_OFF_ID 0
#define IOAPIC_OFF_MAX_IRQS 4
#define IOAPIC_OFF_REDTBL 8

#define QUEUE_MAX_INTERRUPTS 256

#define PADDR_IOAPIC 0xFEC00000

union redirTableEntry {
  uint32_t val;
  struct {
    uint32_t vector : 16;
    uint32_t dest_cpu : 16;
  } fields;
};

bool apicAccess(vcpu_t *vcpu, uint64_t phys_addr);
int apicMmio(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data, uint32_t len, uint8_t is_write);
bool lapicAccess(vcpu_t *, uint64_t);
int lapicMmio(vcpu_t *, uint64_t, uint64_t *, uint32_t, uint8_t);
ioapic_t * initIoApic(hv_t *hv);
void * ioApicThread(void *arg);
bool ioApicAccess(uint64_t);
int ioApicMmio(vcpu_t *vcpu, uint64_t phys_addr, uint64_t *data, uint32_t len, uint8_t is_write);

int queueInterrupt(ioapic_t *ioapic, uint8_t irq);
struct interrupt_entry * dequeueInterrupt(ioapic_t *ioapic);
bool haveInterrupts(ioapic_t *ioapic);
int checkAndSendInterrupt(hv_t *hv, vcpu_t *vcpu);

#endif
