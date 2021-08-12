#pragma once

#include <functional>

// #define TRACE

#define MB(x)   ((size_t) (x) << 20)
#define NUM_THREADS 4

#define MAX_ETHERNET_SIZE 1518


#ifdef TRACE
#include <stdio.h>

extern const char* opcode_to_name[];
extern const char* rot_to_name[];
extern const char* register_to_name[];
extern const char* alu_op_to_name[];
extern const char* shift_to_name[];
extern const char* direction_to_name[];
extern const char* token_to_name[];
extern const char* ctx_to_color[];
#endif


#ifdef TRACE
  #define TRACE_PRINT( format, ... ) printf( "%15.15s::%16.16s(%d) \t" format "\n", __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__ )
#else
  #define TRACE_PRINT( format, ... )
#endif

typedef enum {
   /* Start ALU instructions */
   ALU, /* Perform an artihmetic operation (ALU command specific as an op) */
   ALU_SHF, /* Arithmetic operation and shift */
   DBL_SHF, /* Concatenate and shift two longwords */
   /* End ALU instructions */
   /* Start Branching instructions */
   BR,
   BR_EQ, /* src_1 == src_2 */
   BR_NEQ, /* src_1 != src_2 */
   BR_LESS, /* src_1 < src_2 */
   BR_LESS_EQ, /* src_1 <= src_2 */
   BR_GREATER, /* src_1 > src_2 */
   BR_GREATER_EQ, /* src_1 >= src_2 */
   BR_EQ_COUNT,
   BR_NEQ_COUNT,
   BR_BSET, /* Branch if bit set  */
   BR_BCLR, /* Branch if bit clear */
   BR_EQ_BYTE, /* Branch if byte equal */
   BR_NEQ_BYTE, /* Branch if byte not equal */
   BR_EQ_CTX, /* Branch if context (thread) equals src_1 */
   BR_NEQ_CTX, /* Branch if context (tread) not equals src_1 */
   BR_INP_STATE, /* Branch on event state (possibly unused) */
   BR_NOT_SIGNAL, /* Branch if signal deasserted (probably unused) */
   RTN, /* Return from branch or jump (WTF is this) */
   /* End Branching instructions */
   /* Start Reference instructions */
   CSR, /* Access a CSR register (reference) */
   FAST_WR, /* Write immediate values to thread-local CSRs (fast so avoids reference CSR) */
   LOCAL_CSR_RD, /* Read from a local CSR (reference) */
   RX_PKT, /* Received packet for co-processor to process */
   R_FIFO_RD, /* Read from the RX FIFO (reference) */
   SCRATCH, /* Scratchpad memory request (reference) */
   RAM, /* RAM (reference) */
   T_FIFO_WR, /* Write to the TX FIFO (reference) */
   /* End Reference instructions */
   /* Start Immediate Instructions */
   IMMED, /* Load immediate value and sign extend */
   IMMED_B0, /* Load immediate byte into first byte of register */
   IMMED_B1, /* Load immediate byte into second byte of register */
   IMMED_B2, /* load immediate byte into third byte of register */
   IMMED_B3, /* load immediate byte into fourth byte of register */
   IMMED_W0, /* load immediate word into first word of register */
   IMMED_W1, /* load immediate word into second word of register */
   LD_FIELD, /* Load bytes from src to dst according to mask (shift before mask) */
   LD_FIELD_W_CLR, /* Load bytes from src to dst (clear dst) according to mask (shift before mask)example is: ld_field_w_clr [dport, 0011, $$hdr0, >>16], and shift comes before the mask ?? */
   LOAD_ADDR, /* Load instruction address */
   /* End Immediate Instructions */
   CTX_ARB, /* Perform context swap (yield control) and tell when to wake */
   NOP, /* Do nothing */
   HASH1_48, /* Perform 48-bit hash function 1 */
   HASH2_48, /* Perform 48-bit hash function 2 */
   HASH3_48, /* Perform 48-bit hash function 3 */
   HASH1_64, /* Perform 64-bit hash function 1 */
   HASH2_64, /* Perform 64-bit hash function 2 */
   HASH3_64, /* Perform 64-bit hash function 3 */
} opcode_type;

typedef enum {
   PLUS,
   MINUS, /* src_1 - src_2 */
   BACKWARDS_MINUS, /* src_2 - src_1 */
   SECOND, /* src_2 */
   BIT_NOT_SECOND, /* bitwise inversion of src_2 */
   AND, /* src_1 bitwise_and src_2 */
   OR, /* src_1 bitwise_or src_2 */
   XOR, /* src_1 bitwise_xor src_2 */
   PLUS_CARRY, /* src_1 + src_2 + carry from previous operation */
   ALU_SHIFT_LEFT, /* src_1<<src_2 */
   ALU_SHIFT_RIGHT, /* src_1>>src_2 */
   PLUS_IF_SIGN, /* If prior operation has a sign condition, the return src_1 + src_2, otherwise src_2 */
   PLUS_FOUR, /* src_1 + src_2  & 0xF (last four bits survive) */
   PLUS_EIGHT, /* src_1 + src_2  & 0xFF (last eight bits survive) */
   PLUS_SIXTEEN, /* src_1 + src_2  & 0xFFFF (last 16 bits survive) */
} alu_ops_type;

typedef enum {
   READ,
   WRITE,
} direction_type;

typedef enum {
   NO_TOKEN,
   CTX_SWAP, /* Swap this thread out until operation completes */
   SIG_DONE, /* wait for signal (likely get rid of) */
} token_type;

typedef enum {
   NO_ROT,
   ANOTHER_NO_ROT,
   LEFT_EIGHT,
   LEFT_SIXTEEN,
} rot_type;

typedef enum {
   SHIFT_LEFT,
   SHIFT_RIGHT,
} shift_type;

typedef enum {
   RELATIVE,
   ABSOLUTE,
} reg_ref_type;

typedef uint8_t register_ref;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type dst_type:1;
   register_ref dst:8;
   uint16_t ival:16;
   rot_type rot:2;
} immediate;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type dst_type:1;
   register_ref dst:8;
   uint16_t csr_num:16;
} csr;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type dst_type:1;
   register_ref dst:8;
   reg_ref_type src_1_type:1;
   register_ref src_1:8;
   alu_ops_type type:4;
   reg_ref_type src_2_type:1;
   register_ref src_2:8;
   shift_type shift:1;
   uint8_t num_shift:2;
} alu;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   direction_type direction:1;
   reg_ref_type xfer_reg_type:1;
   register_ref xfer_reg:8;
   reg_ref_type addr_1_type:1;
   register_ref addr_1:8;
   reg_ref_type addr_2_type:1;
   register_ref addr_2:8;
   uint8_t count:4;
   token_type token:2;
} memory;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type src_1_type:1;
   register_ref src_1:8;
   reg_ref_type src_2_type:1;
   register_ref src_2:8;
   uint16_t target:12;
} branch;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type dst_type:1;
   register_ref dst:8;
   uint8_t mask:4;
   reg_ref_type src_type:1;
   register_ref src:8;
   rot_type rot:2;
} load;

typedef struct __attribute__((packed)) {
   opcode_type opcode:6;
   reg_ref_type size_type:1;
   register_ref size:8;
   reg_ref_type addr_1_type:1;
   register_ref addr_1:8;
   reg_ref_type addr_2_type:1;
   register_ref addr_2:8;
   token_type token:2;
} fifo;


typedef union __attribute__((packed)) {
   opcode_type opcode:6;
   alu alu_inst;
   immediate immediate_inst;
   memory memory_inst;
   branch branch_inst;
   load load_inst;
   fifo fifo_inst;
   csr csr_inst;
} instruction;

typedef uint32_t register_type;


// Struct that's used to request reads/writes to memory
typedef struct {
   direction_type direction;
   uint32_t mem_offset;
   register_type* start_reg;
   uint8_t count;
   std::function<void (void)> callback;
} memory_job;

// Struct that's used to request TX/RX to fifo physical device
typedef struct {
   register_type* size;
   uint8_t* start;
   std::function<void (void)> callback;
} fifo_job;
