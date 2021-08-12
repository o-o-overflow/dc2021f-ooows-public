#include <stdlib.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <chrono>

#include "microengine.hpp"

#include "phy.hpp"


int main(int argc, char**argv)
{
   instruction test[1024];
   int fd = open(argv[1], 0);
   int size = read(fd, (void*)test, sizeof(test));

   uint32_t* ram = (uint32_t*)calloc(MB(8), 1);


   ThreadedQueue<fifo_job> tx_queue;
   ThreadedQueue<fifo_job> rx_queue;
   ThreadedQueue<fifo_job> pkt_in_queue;

   Phy* p = new Phy(&tx_queue, &rx_queue);

   Microengine* m = new Microengine((instruction*)test, size, ram, &tx_queue, &rx_queue, &pkt_in_queue);
   uint32_t* scratch = m->m_scratch;


   // Set up some "packets" to send for testing

   char* str = "DSTMACSRCMACLN\x4fGTLIDFOTP\x01\x01IPIPDTDTDTDTAA";
   //char* crashing_input = "DSTMACSRCMACLN\x4fGTLIDFOTP\x01\x01IPIPDTDTDTDTA";
   int str_size = strlen(str);
   strcpy((char*)ram, str);

   auto send_all = [=]() {
      while (true)
      {
         for (int i = 0; i < 16; i++)
         {
            int idx = 2 + (i*2);
            scratch[idx] = 0x80000000 | str_size;
            scratch[idx+1] = 0;
         }
      }};

   auto read_all = [=]() {
      while (true)
      {
         for (int i = 0; i < 16; i++)
         {
            int idx = 34 + (i*2);
            if (scratch[idx] & 0x80000000)
            {
               int pkt_idx = scratch[idx+1];
               printf("%s", ram+pkt_idx);
            }
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(3000));
      }
   };

   //std::thread read_all_thread = std::thread(read_all);

   std::thread send_all_thread = std::thread(send_all);

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

   char mac[6] = {'B', 'C', 'D', 'E', 'F', 'G'};

   m->set_mac_address(mac);
   //m->set_promisc_mode(1);
   //m->set_chksum_tx_offload(1);
   //m->set_tx_eth_crc_offload_mode(1);
   m->m_ctx_ready[0] = false;
   m->m_ctx_ready[1] = false;
   m->m_ctx_ready[2] = false;
   m->m_ctx_ready[3] = true;


   m->interpreter_loop();

   return 0;



}
