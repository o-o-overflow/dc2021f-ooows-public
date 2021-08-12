#include <stdlib.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <chrono>

#include "microengine.hpp"

// TODO: remove, only needed for testing
#include "phy.hpp"

const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

Microengine::Microengine(instruction* code, uint32_t code_len, uint32_t* ram, ThreadedQueue<fifo_job>* tx_queue, ThreadedQueue<fifo_job>* rx_queue, ThreadedQueue<fifo_job>* pkt_in_queue):
   m_done(0),
   m_pc_ctx{0},
   m_current_ctx(0),
   m_registers{0},
   m_code_len(code_len),
   m_flags{0},
   m_scratch{0},
   m_ram(ram),
   m_tx_queue(tx_queue),
   m_rx_queue(rx_queue),
   m_pkt_in_queue(pkt_in_queue),
   m_csr(0)
{

   if (code_len > sizeof(m_code))
   {
      throw std::exception();
   }
   memcpy(m_code, code, code_len);

   // set up the crc32 table at the correct offset
   memcpy(m_ram+0x400, crc32_tab, sizeof(crc32_tab));

   // initialize each thread as ready to go
   for (int i = 0; i < NUM_THREADS; i++)
   {
      m_ctx_ready[i] = true;
   }

   // Start the scrach memory controller
   m_scratch_thread = std::thread(&Microengine::handle_scratch, this);
   m_ram_thread = std::thread(&Microengine::handle_ram, this);
}

void Microengine::set_mac_address(char new_mac[6])
{
   memcpy(m_mac, new_mac, 6);
}

void Microengine::change_csr(bool new_val, register_type mask)
{
   TRACE_PRINT("new_val=%d mask=0x%x csr=0x%x", new_val, mask, m_csr);
   if (new_val)
   {
      m_csr |= mask;
   }
   else
   {
      m_csr &= ~mask;
   }
   TRACE_PRINT("new_csr=0x%x", m_csr);
}

void Microengine::next_context()
{
   m_current_ctx = (m_current_ctx + 1) % NUM_THREADS;
}

void Microengine::thread_ready_to_run()
{
   int i = 0;
   while(true)
   {
      if (m_ctx_ready[m_current_ctx])
      {
         if (i != 0)
         {
            TRACE_PRINT("Thread %d ready to go", m_current_ctx);
         }
         return;
      }
      next_context();
      i += 1;
      if ((i % NUM_THREADS) == 0)
      {
         TRACE_PRINT("%d threads ready, sleep for a bit", 0);
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
   }
}

void Microengine::interpreter_loop()
{
   // test is_memory_*_register
   // for (int i = 0; i < 256; i++)
   // {
   //    printf("%d %s: R:%d W:%d\n", i, register_to_name[i], is_memory_read_register(i, 1), is_memory_write_register(i, 1));
   // }


   while (!m_done)
   {

      // Get a thread to run
      thread_ready_to_run();

      uint32_t current_pc = m_pc_ctx[m_current_ctx];
      #ifdef TRACE
      printf("%s", ctx_to_color[m_current_ctx]);
      #endif
      TRACE_PRINT("pc: %d ctx: %d", current_pc, m_current_ctx);
      m_pc_ctx[m_current_ctx] += 1;

      if (current_pc >= m_code_len)
      {
         m_done = 1;
         break;
      }

      instruction inst = m_code[current_pc];
      interpret_instruction(inst);
      #ifdef TRACE
      // reset color
      printf("\e[0m");
      #endif
   }
}

// Essentially decode the instruction to understand what other function to dispath it to
void Microengine::interpret_instruction(instruction inst)
{
   opcode_type opcode = inst.opcode;
   TRACE_PRINT("Processing opcode %d %s", opcode, opcode_to_name[opcode]);
   if (opcode >= ALU && opcode <= DBL_SHF)
   {
      interpret_alu_instruction(&inst.alu_inst);
   }
   else if (opcode >= IMMED && opcode <= IMMED_W1)
   {
      interpret_immediate_instruction(&inst.immediate_inst);
   }
   else if (opcode == SCRATCH || opcode == RAM)
   {
      interpret_memory_instruction(&inst.memory_inst);
   }
   else if (opcode >= BR && opcode <= BR_NOT_SIGNAL)
   {
      interpret_branch_instruction(&inst.branch_inst);
   }
   else if (opcode == LD_FIELD || opcode == LD_FIELD_W_CLR)
   {
      interpret_load_instruction(&inst.load_inst);
   }
   else if (opcode == T_FIFO_WR || opcode == R_FIFO_RD || opcode == RX_PKT)
   {
      interpret_fifo_instruction(&inst.fifo_inst);
   }
   else if (opcode == NOP)
   {
      TRACE_PRINT("Do nothing for NOP %d", opcode);
   }
   else if (opcode == CTX_ARB)
   {
      TRACE_PRINT("Thread %d wants to yield", m_current_ctx);
      next_context();
   }
   else if (opcode == CSR)
   {
      interpret_csr_instruction(&inst.csr_inst);
   }
   else
   {
      TRACE_PRINT("Unknown opcode: %d", opcode);
   }
}

void Microengine::interpret_alu_instruction(alu* alu_inst)
{
   register_type dst = absolute_register(alu_inst->dst_type, alu_inst->dst);
   register_type src_1 = absolute_register(alu_inst->src_1_type, alu_inst->src_1);
   register_type src_2 = absolute_register(alu_inst->src_2_type, alu_inst->src_2);
   TRACE_PRINT("%s %s = %s %s %s %s%d",
               opcode_to_name[alu_inst->opcode],
               register_to_name[dst],
               register_to_name[src_1],
               alu_op_to_name[alu_inst->type],
               register_to_name[src_2],
               shift_to_name[alu_inst->shift],
               alu_inst->num_shift);
   switch(alu_inst->opcode)
   {
      case ALU:
      case ALU_SHF:
      {
         uint64_t result;
         register_type src_1_val = m_registers[src_1];
         register_type src_2_val = m_registers[src_2];

         switch(alu_inst->type)
         {
            case PLUS:
               result = (uint64_t)src_1_val + (uint64_t)src_2_val;
               break;
            case MINUS:
               result = src_1_val - src_2_val;
               break;
            case BACKWARDS_MINUS:
               result = src_2_val - src_1_val;
               break;
            case SECOND:
               result = src_2_val;
               break;
            case BIT_NOT_SECOND:
               result = ~src_2_val;
               break;
            case AND:
               result = src_1_val & src_2_val;
               break;
            case OR:
               result = src_1_val | src_2_val;
               break;
            case XOR:
               result = src_1_val ^ src_2_val;
               break;
            case PLUS_CARRY:
               result = (uint64_t)src_1_val + (uint64_t)src_2_val + (uint64_t)m_flags.carry;
               break;
            case ALU_SHIFT_LEFT:
               result = src_1_val<<src_2_val;
               break;
            case ALU_SHIFT_RIGHT:
               result = src_1_val>>src_2_val;
               break;
            case PLUS_IF_SIGN:
               if (m_flags.sign)
               {
                  result = (uint64_t)src_1_val + (uint64_t)src_2_val;
               }
               else
               {
                  result = src_2_val;
               }
               break;
            case PLUS_FOUR:
               result = (src_1_val + src_2_val) & 0xF;
               break;
            case PLUS_EIGHT:
               result = (src_1_val + src_2_val) & 0xFF;
               break;
            case PLUS_SIXTEEN:
               result = (src_1_val + src_2_val) & 0xFFFF;
               break;
            default:
               TRACE_PRINT("Unhandled ALU OP %s", alu_op_to_name[alu_inst->type]);
               break;
         }

         // need to set carry
         m_flags.carry = (result & 0x100000000) >> 32;

         // need to set signed
         m_flags.sign = (result & 0x80000000) >> 31;

         // Maybe TODO: should shift affect carry/sign? Let's not for now.
         if (alu_inst->opcode == ALU_SHF)
         {
            uint32_t tmp = (uint32_t)result;
            if (alu_inst->shift == SHIFT_LEFT)
            {
               tmp <<= alu_inst->num_shift;
            }
            else if (alu_inst->shift == SHIFT_RIGHT)
            {
               tmp >>= alu_inst->num_shift;
            }
            result = tmp;
         }

         m_registers[dst] = (uint32_t)result;

         // remember the last alu value, used for indirect references
         m_last_alu = m_registers[dst];
         TRACE_PRINT("Result: 0x%x carry: %d sign: %d",
                     (uint32_t)result,
                     m_flags.carry,
                     m_flags.sign);
         break;
      }
      case DBL_SHF:
      {
         TRACE_PRINT("TODO DBL_SHF %d", 0);
         break;
      }
   }
}

void Microengine::interpret_immediate_instruction(immediate* immed_inst)
{
   register_ref dst = absolute_register(immed_inst->dst_type, immed_inst->dst);
   TRACE_PRINT("%s %s = 0x%x ROT %s",
               opcode_to_name[immed_inst->opcode],
               register_to_name[dst],
               immed_inst->ival,
               rot_to_name[immed_inst->rot]);

   switch(immed_inst->opcode)
   {
      case IMMED:
      {
         int32_t value = (int32_t)immed_inst->ival;
         m_registers[dst] = perform_rot(value, immed_inst->rot);
         break;
      }
      case IMMED_B0:
      {
         register_type value = immed_inst->ival & 0xff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0xffffff00) + value;
         m_registers[dst] = new_value;
         break;
      }
      case IMMED_B1:
      {
         register_type value = immed_inst->ival & 0xff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0xffff00ff) + (value << 8);
         m_registers[dst] = new_value;
         break;
      }
      case IMMED_B2:
      {
         register_type value = immed_inst->ival & 0xff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0xff00ffff) + (value << 16);
         m_registers[dst] = new_value;
         break;
      }
      case IMMED_B3:
      {
         register_type value = immed_inst->ival & 0xff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0x00ffffff) + (value << 24);
         m_registers[dst] = new_value;
         break;
      }
      case IMMED_W0:
      {
         register_type value = immed_inst->ival & 0xffff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0xffff0000) + (value);
         m_registers[dst] = new_value;
         break;
      }
      case IMMED_W1:
      {
         register_type value = immed_inst->ival & 0xffff;
         register_type orig = m_registers[dst];
         register_type new_value = (orig & 0x0000ffff) + (value << 16);
         m_registers[dst] = new_value;
         break;
      }





   }
}

// thread that handles all the ram access
void Microengine::handle_ram(void)
{
   while (true)
   {
      ThreadedQueue<memory_job>* queue = &m_ram_queue;
      uint32_t* mem = m_ram;
      memory_job job = queue->get();

      if (job.direction == WRITE)
      {
         for (int i = 0; i < job.count; i++)
         {
            mem[job.mem_offset+i] = job.start_reg[i];
            TRACE_PRINT("Writing %d 0x%x to %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else if (job.direction == READ)
      {
         for (int i = 0; i < job.count; i++)
         {
            job.start_reg[i] = mem[job.mem_offset+i];
            TRACE_PRINT("Reading %d 0x%x from %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else
      {
         TRACE_PRINT("Unhandled mem direction %d", job.direction);
      }

      if (job.callback)
      {
         job.callback();
      }
   }

}


// thread that handles all the scratch
void Microengine::handle_scratch(void)
{
   // handle_memory(&m_scratch_queue, m_scratch);
   while (true)
   {
      ThreadedQueue<memory_job>* queue = &m_scratch_queue;
      uint32_t* mem = m_scratch;
      memory_job job = queue->get();

      if (job.direction == WRITE)
      {
         for (int i = 0; i < job.count; i++)
         {
            mem[job.mem_offset+i] = job.start_reg[i];
            TRACE_PRINT("Writing %d 0x%x to %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else if (job.direction == READ)
      {
         for (int i = 0; i < job.count; i++)
         {
            job.start_reg[i] = mem[job.mem_offset+i];
            TRACE_PRINT("Reading %d 0x%x from %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else
      {
         TRACE_PRINT("Unhandled mem direction %d", job.direction);
      }

      if (job.callback)
      {
         job.callback();
      }
   }

}

void Microengine::handle_memory(ThreadedQueue<memory_job>* queue, uint32_t* mem)
{
   while (true)
   {
      memory_job job = queue->get();

      if (job.direction == WRITE)
      {
         for (int i = 0; i < job.count; i++)
         {
            mem[job.mem_offset+i] = job.start_reg[i];
            TRACE_PRINT("Writing %d 0x%x to %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else if (job.direction == READ)
      {
         for (int i = 0; i < job.count; i++)
         {
            job.start_reg[i] = mem[job.mem_offset+i];
            TRACE_PRINT("Reading %d 0x%x from %p", job.mem_offset+i, job.start_reg[i], &mem[job.mem_offset+i]);
         }
      }
      else
      {
         TRACE_PRINT("Unhandled mem direction %d", job.direction);
      }

      if (job.callback)
      {
         job.callback();
      }
   }
}

void Microengine::interpret_memory_instruction(memory* memory_inst)
{
   register_ref xfer_reg = absolute_register(memory_inst->xfer_reg_type, memory_inst->xfer_reg);
   register_ref addr_1 = absolute_register(memory_inst->addr_1_type, memory_inst->addr_1);
   register_ref addr_2 = absolute_register(memory_inst->addr_2_type, memory_inst->addr_2);

   uint8_t count = memory_inst->count;
   // if count is zero, this is an indirect memory reference, the count is the last value through the alu
   if (count == 0)
   {
      count = m_last_alu;
   }

   TRACE_PRINT("%s %s %s addr %s:%d + %s:%d count %d %s",
               opcode_to_name[memory_inst->opcode],
               direction_to_name[memory_inst->direction],
               register_to_name[xfer_reg],
               register_to_name[addr_1],
               m_registers[addr_1],
               register_to_name[addr_2],
               m_registers[addr_2],
               count,
               token_to_name[memory_inst->token]);

   if ((memory_inst->direction == READ && !is_memory_read_register(xfer_reg, count)) \
       || (memory_inst->direction == WRITE && !is_memory_write_register(xfer_reg, count)))
   {
      TRACE_PRINT("Invalid %s register %s count %d",
                  direction_to_name[memory_inst->direction],
                  register_to_name[xfer_reg],
                  count);
      return;
   }

   uint32_t memory_offset = m_registers[addr_1] + m_registers[addr_2];

   // If the token is CTX_SWAP, tell that we're disabled
   if (memory_inst->token == CTX_SWAP)
   {
      TRACE_PRINT("Disabling thread %d b/c of CTX_SWAP", m_current_ctx);
      m_ctx_ready[m_current_ctx] = 0;
   }

   switch(memory_inst->opcode)
   {
      case SCRATCH:
      {
         memory_job job = {
            .direction = memory_inst->direction,
            .mem_offset = memory_offset,
            .start_reg = &m_registers[xfer_reg],
            .count = count,
            .callback = NULL,
         };
         if (memory_inst->token == CTX_SWAP)
         {
            auto current_ctx = m_current_ctx;
            job.callback = [=]{ m_ctx_ready[current_ctx] = 1; TRACE_PRINT("Scratch CB: Thread %d now ready", current_ctx); };
         }
         m_scratch_queue.put(job);
         break;
      }
      case RAM:
      {
         memory_job job = {
            .direction = memory_inst->direction,
            .mem_offset = memory_offset,
            .start_reg = &m_registers[xfer_reg],
            .count = count,
            .callback = NULL,
         };
         if (memory_inst->token == CTX_SWAP)
         {
            auto current_ctx = m_current_ctx;
            job.callback = [=]{ m_ctx_ready[current_ctx] = 1; TRACE_PRINT("RAM CB: Thread %d now ready", current_ctx); };
         }
         m_ram_queue.put(job);
         break;
      }

   }
   // Now that we've issues the request if the token is CTX_SWAP, swap us out
   if (memory_inst->token == CTX_SWAP)
   {
      next_context();
   }
}

void Microengine::interpret_branch_instruction(branch* branch_inst)
{
   register_ref src_1 = absolute_register(branch_inst->src_1_type, branch_inst->src_1);
   register_ref src_2 = absolute_register(branch_inst->src_2_type, branch_inst->src_2);
   TRACE_PRINT("%s %s:0x%x %s:0x%x 0x%x",
               opcode_to_name[branch_inst->opcode],
               register_to_name[src_1],
               m_registers[src_1],
               register_to_name[src_2],
               m_registers[src_2],
               branch_inst->target);

   bool goto_target = false;
   switch (branch_inst->opcode)
   {
      case BR:
         goto_target = true;
         break;
      case BR_EQ:
         goto_target = m_registers[src_1] == m_registers[src_2];
         break;
      case BR_NEQ:
         goto_target = m_registers[src_1] != m_registers[src_2];
         break;
      case BR_LESS:
         goto_target = (int32_t)m_registers[src_1] < (int32_t)m_registers[src_2];
         break;
      case BR_LESS_EQ:
         goto_target = (int32_t)m_registers[src_1] <= (int32_t)m_registers[src_2];
         break;
      case BR_GREATER:
         goto_target = (int32_t)m_registers[src_1] > (int32_t)m_registers[src_2];
         break;
      case BR_GREATER_EQ:
         goto_target = (int32_t)m_registers[src_1] >= (int32_t)m_registers[src_2];
         break;
      case BR_EQ_COUNT:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_NEQ_COUNT:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_BSET:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_BCLR:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_EQ_BYTE:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_NEQ_BYTE:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_EQ_CTX:
         goto_target = m_registers[src_1] == m_current_ctx;
         break;
      case BR_NEQ_CTX:
         goto_target = m_registers[src_1] != m_current_ctx;
         break;
      case BR_INP_STATE:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
      case BR_NOT_SIGNAL:
         TRACE_PRINT("NOT IMPLEMENTED %s", opcode_to_name[branch_inst->opcode]);
         break;
   }

   if (goto_target)
   {
      m_pc_ctx[m_current_ctx] = branch_inst->target;
   }
}

void Microengine::interpret_load_instruction(load* load_inst)
{
   register_ref dst = absolute_register(load_inst->dst_type, load_inst->dst);
   register_ref src = absolute_register(load_inst->src_type, load_inst->src);

   TRACE_PRINT("%s %s = %s mask 0x%x shift %s",
               opcode_to_name[load_inst->opcode],
               register_to_name[dst],
               register_to_name[src],
               load_inst->mask,
               rot_to_name[load_inst->rot]);

   if (!(load_inst->opcode == LD_FIELD || load_inst->opcode == LD_FIELD_W_CLR))
   {
      TRACE_PRINT("Unsurported load inst %s", opcode_to_name[load_inst->opcode]);
      return;
   }
   // the only difference between LD_FIELD_W_CLR and LD_FIELD is that
   // we CLEAR the dst in the first part, so let's do that now (the
   // rest of the logic is the same)
   if (load_inst->opcode == LD_FIELD_W_CLR)
   {
      m_registers[dst] = 0;
   }

   register_type val = m_registers[src];

   // perform any shifts necessary
   val = perform_rot(val, load_inst->rot);

   uint32_t bitmask = bytemask_to_bitmask(load_inst->mask);

   register_type orig = m_registers[dst];

   register_type new_dst = (orig & (~bitmask)) ^ (val & bitmask);

   TRACE_PRINT("%s = 0x%x",
               register_to_name[dst],
               new_dst);
   m_registers[dst] = new_dst;

}

void Microengine::interpret_csr_instruction(csr* csr_inst)
{
   register_ref dst = absolute_register(csr_inst->dst_type, csr_inst->dst);
   TRACE_PRINT("%s dst:%s CSR_%d",
               opcode_to_name[csr_inst->opcode],
               register_to_name[dst],
               csr_inst->csr_num);

   register_type result = m_registers[dst];
   // CSR_0 is the standard CSR that we keep track of
   if (csr_inst->csr_num == 0)
   {
      result = m_csr;
   }
   // CSR_1 is the upper 32 bits of the MAC
   else if (csr_inst->csr_num == 1)
   {
      memcpy(&result, m_mac, sizeof(result));
   }
   // CSR_2 is the lower 16 bits of the MAC (in the upper 16 bits)
   else if (csr_inst->csr_num == 2)
   {
      result = 0;
      memcpy(&result, m_mac+4, 2);
   }
   m_registers[dst] = result;
   TRACE_PRINT("Result: %s = 0x%x", register_to_name[dst], result);
}

void Microengine::interpret_fifo_instruction(fifo* fifo_inst)
{
   register_ref size = absolute_register(fifo_inst->size_type, fifo_inst->size);
   register_ref addr_1 = absolute_register(fifo_inst->addr_1_type, fifo_inst->addr_1);
   register_ref addr_2 = absolute_register(fifo_inst->addr_2_type, fifo_inst->addr_2);

   TRACE_PRINT("%s size:%s:%d @ %s:%d + %s:%d %s",
               opcode_to_name[fifo_inst->opcode],
               register_to_name[size],
               m_registers[size],
               register_to_name[addr_1],
               m_registers[addr_1],
               register_to_name[addr_2],
               m_registers[addr_2],
               token_to_name[fifo_inst->token]);

   if (fifo_inst->token == CTX_SWAP)
   {
      TRACE_PRINT("Disabling thread %d b/c of CTX_SWAP", m_current_ctx);
      m_ctx_ready[m_current_ctx] = 0;
   }

   fifo_job job = {
      .size = &m_registers[size],
      .start = (uint8_t*)(m_ram + (m_registers[addr_1] + m_registers[addr_2])),
      .callback = NULL,
   };
   if (fifo_inst->token == CTX_SWAP)
   {
      auto current_ctx = m_current_ctx;
      auto opcode = fifo_inst->opcode;
      job.callback = [=]{ m_ctx_ready[current_ctx] = 1; TRACE_PRINT("%s CB: Thread %d now ready", opcode_to_name[opcode], current_ctx); };
   }

   if (fifo_inst->opcode == T_FIFO_WR)
   {
      m_tx_queue->put(job);
   }
   else if (fifo_inst->opcode == R_FIFO_RD)
   {
      m_rx_queue->put(job);
   }
   else if (fifo_inst->opcode == RX_PKT)
   {
      m_pkt_in_queue->put(job);
   }

   // Now that we've issues the request if the token is CTX_SWAP, swap us out
   if (fifo_inst->token == CTX_SWAP)
   {
      next_context();
   }
}

uint32_t Microengine::bytemask_to_bitmask(uint8_t bytemask)
{
   if (bytemask > 0xf)
   {
      TRACE_PRINT("ERROR: Bad bytemask %x", bytemask);
   }

   uint32_t bitmask = 0;
   for (int i = 0; i < 4; i++)
   {
      // shift bitmask down 8 bits
      bitmask >>= 8;

      if (bytemask & 0x1)
      {
         bitmask ^= 0xff000000;
      }

      // shift bytemask down one bit
      bytemask >>= 1;
   }

   return bitmask;
}

register_type Microengine::perform_rot(register_type val, rot_type rot)
{
   switch(rot)
   {
      case LEFT_EIGHT:
         return val <<= 8;
         break;
      case LEFT_SIXTEEN:
         return val <<= 16;
         break;
      default:
         return val;
         break;
   }
}

register_ref Microengine::absolute_register(reg_ref_type ref_type, register_ref reg)
{
   if (ref_type == ABSOLUTE)
   {
      return reg;
   }
   else if (ref_type == RELATIVE)
   {
      if (reg < 32)
      {
         return (reg + (m_current_ctx*32)) % sizeof(m_registers);
      }
      else if (reg >= 32 && reg < 48)
      {
         register_ref start_reg = (reg - 32) + 128;
         return (start_reg + (m_current_ctx*16)) % sizeof(m_registers);
      }
      else
      {
         register_ref start_reg = (reg - 48) + 192;
         return (start_reg + (m_current_ctx*16)) % sizeof(m_registers);
      }

   }
}

bool Microengine::is_memory_read_register(register_ref reg, uint8_t count)
{
   // read registers start at 192 and go until 256
   register_ref start = reg;
   // register_ref end = reg+count-1;

   return start >= 192 && start < 256;
}

bool Microengine::is_memory_write_register(register_ref reg, uint8_t count)
{
   // write registers start at 128 and go until 192
   register_ref start = reg;
   // register_ref end = reg+count-1;

   return start >= 128 && start < 192;
}


// int main(int argc, char**argv)
// {

//    instruction i[] = {{
//          .immediate_inst = {
//             .opcode = IMMED,
//             .dst_type = RELATIVE,
//             .dst = 0x0,
//             .ival = 0xFFFF,
//             .rot = LEFT_SIXTEEN,
//          }
//       },{
//          .immediate_inst = {
//             .opcode = IMMED,
//             .dst_type = RELATIVE,
//             .dst = 0x1,
//             .ival = 0xFFFF,
//             .rot = NO_ROT,
//          }
//       },{
//          .immediate_inst = {
//             .opcode = IMMED,
//             .dst_type = RELATIVE,
//             .dst = 0x2,
//             .ival = 0x1,
//             .rot = NO_ROT,
//          }
//       },{
//          .immediate_inst = {
//             .opcode = IMMED,
//             .dst_type = RELATIVE,
//             .dst = 0x3,
//             .ival = 0x1,
//             .rot = NO_ROT,
//          }
//       },{
//          .load_inst = {
//             .opcode = LD_FIELD,
//             .dst_type = RELATIVE,
//             .dst = 0x3,
//             .mask = 0x2,
//             .src_type = RELATIVE,
//             .src = 0x1,
//             .rot = NO_ROT,
//          }
//       },{
//          .alu_inst = {
//             .opcode = ALU,
//             .dst_type = RELATIVE,
//             .dst = 0x3,
//             .src_1_type = RELATIVE,
//             .src_1 = 0x0,
//             .type = PLUS,
//             .src_2_type = RELATIVE,
//             .src_2 = 0x1,
//          }
//       },{
//          .alu_inst = {
//             .opcode = ALU,
//             .dst_type = RELATIVE,
//             .dst = 0x4,
//             .src_1_type = RELATIVE,
//             .src_1 = 0x3,
//             .type = PLUS,
//             .src_2_type = RELATIVE,
//             .src_2 = 0x2,
//          }
//       },{
//          .alu_inst = {
//             .opcode = ALU,
//             .dst_type = RELATIVE,
//             .dst = 0x5,
//             .src_1_type = RELATIVE,
//             .src_1 = 0x4,
//             .type = PLUS_CARRY,
//             .src_2_type = RELATIVE,
//             .src_2 = 0x1,
//          }
//       },{
//          .alu_inst = {
//             .opcode = ALU,
//             .dst_type = RELATIVE,
//             .dst = 10,
//             .src_1_type = RELATIVE,
//             .src_1 = 0,
//             .type = SECOND,
//             .src_2_type = RELATIVE,
//             .src_2 = 0x5,
//          }
//       },{
//          .memory_inst = {
//             .opcode = SCRATCH,
//             .direction = WRITE,
//             .xfer_reg_type = RELATIVE,
//             .xfer_reg = 10,
//             .addr_1_type = RELATIVE,
//             .addr_1 = 2,
//             .addr_2_type = RELATIVE,
//             .addr_2 = 2,
//             .count = 1,
//             .token = CTX_SWAP,
//          }
//       },{
//          .memory_inst = {
//             .opcode = SCRATCH,
//             .direction = READ,
//             .xfer_reg_type = RELATIVE,
//             .xfer_reg = 32,
//             .addr_1_type = RELATIVE,
//             .addr_1 = 2,
//             .addr_2_type = RELATIVE,
//             .addr_2 = 2,
//             .count = 1,
//             .token = CTX_SWAP,
//          }
//       },{
//          .opcode = NOP,
//       },{
//          .opcode = CTX_ARB,
//       },{
//          .memory_inst = {
//             .opcode = RAM,
//             .direction = WRITE,
//             .xfer_reg_type = RELATIVE,
//             .xfer_reg = 48,
//             .addr_1_type = RELATIVE,
//             .addr_1 = 2,
//             .addr_2_type = RELATIVE,
//             .addr_2 = 2,
//             .count = 1,
//             .token = CTX_SWAP,
//          }
//       },{
//          .memory_inst = {
//             .opcode = RAM,
//             .direction = READ,
//             .xfer_reg_type = RELATIVE,
//             .xfer_reg = 32,
//             .addr_1_type = RELATIVE,
//             .addr_1 = 2,
//             .addr_2_type = RELATIVE,
//             .addr_2 = 2,
//             .count = 1,
//             .token = CTX_SWAP,
//          }
//       },{
//          .fifo_inst = {
//             .opcode = R_FIFO_RD,
//             .size_type = RELATIVE,
//             .size = 2,
//             .addr_1_type = RELATIVE,
//             .addr_1 = 2,
//             .addr_2_type = RELATIVE,
//             .addr_2 = 2,
//             .token = CTX_SWAP,
//          }
//       },{
//          .branch_inst = {
//             .opcode = BR_EQ,
//             .src_1_type = RELATIVE,
//             .src_1 = 0,
//             .src_2_type = RELATIVE,
//             .src_2 = 0,
//             .target = 0,
//          }
//       }
//    };

//    uint32_t* scratch = (uint32_t*)calloc(1024*sizeof(uint32_t), 1);
//    uint32_t* ram = (uint32_t*)calloc(MB(8), 1);


//    ThreadedQueue<fifo_job> tx_queue;
//    ThreadedQueue<fifo_job> rx_queue;
//    ThreadedQueue<fifo_job> pkt_in_queue;

//    Phy* p = new Phy(&tx_queue, &rx_queue);

//    instruction test[1024];

//    int fd = open(argv[1], 0);

//    int size = read(fd, (void*)test, 1024);

//    // Set up some "packets" to send for testing

//    char* str = "aaaabaaacaaadaaaeaaafaaagaaahaaaiaaajaaakaaalaaamaaanaaaoaaapaaa";
//    int str_size = strlen(str);
//    strcpy((char*)ram, str);

//    auto send_all = [=]() {
//       while (true)
//       {
//          for (int i = 0; i < 16; i++)
//          {
//             int idx = 2 + (i*2);
//             scratch[idx] = 0x80000000 | 4;
//             scratch[idx+1] = i;
//          }
//       }};

//    auto read_all = [=]() {
//       while (true)
//       {
//          for (int i = 0; i < 16; i++)
//          {
//             int idx = 34 + (i*2);
//             if (scratch[idx] & 0x80000000)
//             {
//                int pkt_idx = scratch[idx+1];
//                printf("%s", ram+pkt_idx);
//             }
//          }
//          std::this_thread::sleep_for(std::chrono::milliseconds(3000));
//       }
//    };

//    //std::thread read_all_thread = std::thread(read_all);

//    // std::thread test_thread = std::thread(send_all);

//    auto process_pkt_in = [=, &pkt_in_queue]() {
//       int i = 0;
//       while(true)
//       {
//          fifo_job job = pkt_in_queue.get();
//          job.start[*job.size] = '\0';
//          TRACE_PRINT("Core received PKT: %s", (char*)job.start);

//          // mess with the packet, increment everything by one
//          for (int j = 0; j < *job.size; j++)
//          {
//             char c = ((char*)job.start)[j];
//             ((char*)job.start)[j] = toupper(c);
//          }

//          // put packet on the tx queue
//          int idx = 2 + (i*2);
//          int micro_mem_location = ((char*)job.start - (char*)ram) / 4;
//          scratch[idx+1] = micro_mem_location;
//          scratch[idx] = 0x80000000 | *job.size;
//          i += 1;
//          i = i % 16;

//          if (job.callback)
//          {
//             job.callback();
//          }
//       }
//    };

//    std::thread process_pkt_thread = std::thread(process_pkt_in);

//    Microengine* m = new Microengine((instruction*)test, size, scratch, ram, &tx_queue, &rx_queue, &pkt_in_queue);
//    m->m_ctx_ready[0] = true;
//    m->m_ctx_ready[1] = true;
//    m->m_ctx_ready[2] = true;
//    m->m_ctx_ready[3] = true;


//    m->interpreter_loop();

//    return 0;
// }
