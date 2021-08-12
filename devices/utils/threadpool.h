#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <stdlib.h>
#include <sys/types.h>

typedef struct work work_t;

typedef struct work {
  work_t *flink;
  work_t *blink;
  unsigned char data[];
} work_t;

typedef struct queue {
  size_t len;
  work_t *flink;
  work_t *blink;
} queue_t;

typedef struct threadpool {
  pthread_t *workers;
  int (*routine)(void *);
  pthread_mutex_t work_mutex;
  pthread_cond_t work_ready;
  queue_t queue;
} threadpool_t;

threadpool_t *create_threadpool(size_t n, int (*routine)(void *));
int queue_work(threadpool_t *tp, void *data, size_t sz);
#endif
