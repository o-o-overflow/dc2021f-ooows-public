#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "threadpool.h"

work_t *dequeue_work_unlocked(threadpool_t *tp) {

  work_t *work = tp->queue.flink;

  if (work->flink) {
    work->flink->blink = NULL;
  }

  tp->queue.flink = work->flink;
  if (tp->queue.blink == work) {
    tp->queue.blink = NULL;
  }

  tp->queue.len--;
  work->flink = NULL;

  return work;
}

int queue_work(threadpool_t *tp, void *data, size_t sz) {
  work_t *work = calloc(1, sizeof(work_t) + sz);
  if (work == NULL) {
    return -1;
  }

  memcpy(work->data, data, sz);

  // tail insert the work item
  pthread_mutex_lock(&tp->work_mutex);
  work->flink = NULL;
  work->blink = tp->queue.blink;

  if (tp->queue.blink) {
    tp->queue.blink->flink = work;
  }

  if (tp->queue.flink == NULL) {
    tp->queue.flink = work;
  }

  tp->queue.len++;
  tp->queue.blink = work;

  pthread_cond_signal(&tp->work_ready);

  pthread_mutex_unlock(&tp->work_mutex);

  return 0;
}

void *_routine_stub(void *t) {
  threadpool_t *tp = (threadpool_t *)t;

  // wait while the queue of work is empty
  while (1) {
    pthread_mutex_lock(&tp->work_mutex);
    while (tp->queue.len == 0) {
      pthread_cond_wait(&tp->work_ready, &tp->work_mutex);
    }
    work_t *work = dequeue_work_unlocked(tp);
    pthread_mutex_unlock(&tp->work_mutex);

    tp->routine(work->data);

    free(work);
  }

  return NULL;
}


threadpool_t *create_threadpool(size_t n, int (*routine)(void *)) {

  threadpool_t *tp = calloc(1, sizeof(threadpool_t));
  if (tp == NULL) {
    return NULL;
  }

  // initialize sync primitives
  pthread_mutex_init(&tp->work_mutex, NULL);
  pthread_cond_init(&tp->work_ready, NULL);

  tp->routine = routine;

  tp->workers = calloc(n, sizeof(pthread_t));
  if (tp->workers == NULL) {
    goto err;
  }

  int i = 0;
  for(i=0;i<n;i++) {
    if (pthread_create(&tp->workers[i], NULL, _routine_stub, tp) < 0) {
      perror("pthread_create");
      goto err;
    }
  }

  return tp;
 err:
  printf("erro\n");
  if (tp->workers) {
    for(i=0;i<n;i++) {
      if (tp->workers[i]) pthread_cancel(tp->workers[i]);
    }
    free(tp->workers);
  }

  free(tp);
  return NULL;
}
