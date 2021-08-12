#pragma once

#include <thread>

#include <mosquitto.h>

#include "types.hpp"
#include "myqueue.hpp"

class Phy {

public:
   // Continue to process packets forever, sending those that come on
   // where the rx_queue goes, and sending out those packets that come
   // on the tx_queue.
   Phy(ThreadedQueue<fifo_job>* tx_queue, ThreadedQueue<fifo_job>* rx_queue, const char* host, int port, const char* vpc);
   Phy(ThreadedQueue<fifo_job>* tx_queue, ThreadedQueue<fifo_job>* rx_queue): Phy(tx_queue, rx_queue, "localhost", 1883, "VPC") {};

   bool m_done;
   const char* m_vpc;

   uint32_t m_nic;

   ThreadedQueue<fifo_job>* m_tx_queue;
   ThreadedQueue<fifo_job>* m_rx_queue;


private:

   std::thread m_tx_thread;
   std::thread m_rx_thread;

   void handle_tx(void);
   void handle_rx(void);

   std::thread m_mosquitto;
   struct mosquitto* m_mosq;

   void handle_mosquitto(void);


};
