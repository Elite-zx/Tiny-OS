#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "sync.h"
#include "thread.h"

#define BUF_SIZE 64

/**
 * struct ioqueue - Defines a circular buffer for I/O operations
 * @_lock: A lock to ensure mutual exclusion in accessing the buffer
 * @producer: Pointer to the task_struct representing the buffer's producer
 * @consumer: Pointer to the task_struct representing the buffer's consumer
 * @buf: Array that serves as the circular buffer
 * @head: Index of the buffer's head, indicating the next position for reading
 * @tail: Index of the buffer's tail, indicating the next position for writing
 *
 * Represents a circular buffer used for producer-consumer I/O operations. It
 * includes synchronization mechanisms (a lock) and pointers to task_structs for
 * both producer and consumer. The buffer is implemented as an array with head
 * and tail indices to manage the data flow efficiently.
 */
struct ioqueue {
  struct lock _lock;
  struct task_struct *producer;
  struct task_struct *consumer;
  char buf[BUF_SIZE];
  int32_t head;
  int32_t tail;
};

void ioqueue_init(struct ioqueue *ioq);
bool ioq_is_full(struct ioqueue *ioq);
bool ioq_is_empty(struct ioqueue *ioq);
char ioq_getchar(struct ioqueue *ioq);
void ioq_putchar(struct ioqueue *ioq, char ch);
#endif
