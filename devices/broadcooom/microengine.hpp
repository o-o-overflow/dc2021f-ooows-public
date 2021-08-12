#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <thread>

#include "types.hpp"
#include "myqueue.hpp"

#define PROMISC_MODE_MASK 0x1
#define CHXSUM_RX_OFFLOAD_MASK 0x1<<1
#define CHXSUM_TX_OFFLOAD_MASK 0x1<<2
#define RX_ETH_CRC_CHECK_MASK 0x1<<3
#define TX_ETH_CRC_OFFLOAD_MASK 0x1<<4


class Microengine {
public:
   Microengine(instruction* code, uint32_t code_len, uint32_t* ram, ThreadedQueue<fifo_job>* tx_queue, ThreadedQueue<fifo_job>* rx_queue, ThreadedQueue<fifo_job>* pkt_in_queue);
   ~Microengine();

   void interpreter_loop(void);

   void set_mac_address(char new_mac[6]);
   void set_chksum_tx_offload(bool new_val) { change_csr(new_val, CHXSUM_TX_OFFLOAD_MASK); }
   void set_chksum_rx_offload(bool new_val) { change_csr(new_val, CHXSUM_RX_OFFLOAD_MASK); }
   void set_promisc_mode(bool new_val) { change_csr(new_val, PROMISC_MODE_MASK); }
   void set_rx_eth_crc_check_mode(bool new_val) { change_csr(new_val, RX_ETH_CRC_CHECK_MASK); }
   void set_tx_eth_crc_offload_mode(bool new_val) { change_csr(new_val, TX_ETH_CRC_OFFLOAD_MASK); }


   // ctx means the thread number, used in the instructions
   uint8_t m_current_ctx;
   uint32_t m_pc_ctx[NUM_THREADS];
   uint32_t m_scratch[1024];
   register_type m_csr;
   register_type m_registers[256];
   instruction m_code[1024];


   std::atomic_bool m_ctx_ready[NUM_THREADS];

private:
   uint32_t m_code_len;
   uint8_t m_done;

   char m_mac[6];

   // ALU bits
   struct {
      uint8_t carry:1;
      uint8_t sign:1;
   } m_flags;
   register_type m_last_alu;

   // Memory is not byte-addressable, and must be accessed 4 (or 8 in
   // the case of sdram) bytes at a time, and addresses can only be
   // done on byte boundries.
   uint32_t* m_ram;
   std::thread m_scratch_thread;
   std::thread m_ram_thread;

   ThreadedQueue<memory_job> m_scratch_queue;
   ThreadedQueue<memory_job> m_ram_queue;
   ThreadedQueue<fifo_job>* m_tx_queue;
   ThreadedQueue<fifo_job>* m_rx_queue;
   ThreadedQueue<fifo_job>* m_pkt_in_queue;

   // Function that manages all memory access
   void handle_scratch(void);
   void handle_ram(void);
   static void handle_memory(ThreadedQueue<memory_job>* queue, uint32_t* mem);

   // some helper functions
   register_ref absolute_register(reg_ref_type ref_type, register_ref reg);
   static bool is_memory_read_register(register_ref reg, uint8_t count);
   static bool is_memory_write_register(register_ref reg, uint8_t count);
   static register_type perform_rot(register_type val, rot_type rot);
   static uint32_t bytemask_to_bitmask(uint8_t bytemask);
   void change_csr(bool new_val, register_type mask);

   // Interpreter functions
   void interpret_instruction(instruction inst);
   void interpret_alu_instruction(alu* alu_inst);
   void interpret_immediate_instruction(immediate* immed_inst);
   void interpret_memory_instruction(memory* memory_inst);
   void interpret_branch_instruction(branch* branch_inst);
   void interpret_load_instruction(load* load_inst);
   void interpret_fifo_instruction(fifo* fifo_inst);
   void interpret_csr_instruction(csr* csr_inst);

   // Set the proper context for the next thread that's ready to run
   void thread_ready_to_run();

   // Function to set the context to the next thread
   void next_context();

};
