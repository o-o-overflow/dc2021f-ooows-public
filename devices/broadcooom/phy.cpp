#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <mosquitto.h>

#include "phy.hpp"

void incoming_packet(struct mosquitto* mosq, void* obj, const struct mosquitto_message *message)
{
   Phy* p = (Phy*) obj;
   uint32_t payloadlen = message->payloadlen;

   if (p->m_rx_queue->size() == 0)
   {
      TRACE_PRINT("Ignoring incoming packet w/size %d b/c firmware is not waiting for a packet", payloadlen);
      return;
   }

   if (payloadlen <= 4)
   {
      TRACE_PRINT("Ignoring too-small packet w/size %d", payloadlen);
      return;
   }


   // limit the message size to Ethernet maximum frame size of 1518
   if (payloadlen > MAX_ETHERNET_SIZE)
   {
      TRACE_PRINT("Ignoring large packet w/size %d", payloadlen);
      return;
   }

   // check if the message has our nick at the start, if it came from us
   if (((uint32_t*)message->payload)[0] == p->m_nic)
   {
      TRACE_PRINT("Ignoring packet w/nic %d b/c it's our nic %d", ((uint32_t*)message->payload)[0], p->m_nic);
      return;
   }

   // now, subtract the nic part that was added to the message
   payloadlen -= 4;
   char* payload = ((char*)message->payload) + 4;


   fifo_job job = p->m_rx_queue->get();


   TRACE_PRINT("RX: size:%p:%d start:%p",
               job.size,
               *job.size,
               job.start);

   *job.size = payloadlen;

   memcpy(job.start, payload, payloadlen);

   if (job.callback)
   {
      job.callback();
   }
}

Phy::Phy(ThreadedQueue<fifo_job>* tx_queue, ThreadedQueue<fifo_job>* rx_queue, const char* host, int port, const char* vpc):
   m_tx_queue(tx_queue),
   m_rx_queue(rx_queue),
   m_done(false),
   m_vpc(vpc)
{
   int fd = open("/dev/random", O_RDONLY);
   read(fd, &m_nic, sizeof(m_nic));
   close(fd);

   m_tx_thread = std::thread(&Phy::handle_tx, this);
   //m_rx_thread = std::thread(&Phy::handle_rx, this);

   mosquitto_lib_init();
   bool clean_session = true;
   int keep_alive_seconds = 20;

   m_mosq = mosquitto_new(NULL, clean_session, this);
   mosquitto_message_callback_set(m_mosq, incoming_packet);

   int rc = mosquitto_connect(m_mosq, host, port, keep_alive_seconds);

   mosquitto_subscribe(m_mosq, NULL, vpc, 0);

   m_rx_thread = std::thread([this]{ mosquitto_loop_forever(m_mosq, -1, 1); });
}

void Phy::handle_tx(void)
{
   while(true)
   {
      // TODO
      fifo_job job = m_tx_queue->get();

      TRACE_PRINT("TX: size:%p:%d start:%p",
                  job.size,
                  *job.size,
                  job.start);

      int payloadlen = *job.size + 4;
      char* to_send = (char*)malloc(payloadlen);
      ((uint32_t*)to_send)[0] = m_nic;
      memcpy(to_send+4, job.start, *job.size);

      int rc = mosquitto_publish(m_mosq, NULL, m_vpc, payloadlen, to_send, 0, false);
      TRACE_PRINT("TX: rc=0x%x content=%s", rc, job.start);
      free(to_send);

      if (job.callback)
      {
         job.callback();
      }
   }
}
