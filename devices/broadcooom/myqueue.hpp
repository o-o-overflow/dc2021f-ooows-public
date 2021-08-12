#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

template <class T>
class ThreadedQueue {

public:

   // Put an item on the queue
   void put(T item);

   // Blocks until an item is present
   T get();

   // Get current size
   int size();

private:
   std::queue<T> m_queue;
   std::mutex m_mutex;
   std::condition_variable m_cv;
};


template <class T>
void ThreadedQueue<T>::put(T item)
{
   {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_queue.push(item);
   }
   m_cv.notify_one();
}

template <class T>
T ThreadedQueue<T>::get()
{
   std::unique_lock<std::mutex> lock(m_mutex);
   m_cv.wait(lock, [&]{ return !m_queue.empty(); });
   auto to_return = m_queue.front();
   m_queue.pop();
   return to_return;
}

template <class T>
int ThreadedQueue<T>::size()
{
   std::unique_lock<std::mutex> lock(m_mutex);
   return m_queue.size();
}
