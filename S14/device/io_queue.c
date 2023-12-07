#include "io_queue.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "sync.h"
#include "thread.h"

/**
 * ioqueue_init - Initializes an I/O queue
 * @ioq: Pointer to the ioqueue to initialize
 *
 * Sets up an I/O queue by initializing its lock, setting both producer and
 * consumer to NULL, and resetting head and tail indices.
 */
void ioqueue_init(struct ioqueue *ioq) {
  lock_init(&ioq->_lock);
  ioq->consumer = ioq->producer = NULL;
  ioq->head = ioq->tail = 0;
}

/**
 * next_pos - Calculates the next position in the buffer
 * @pos: Current position in the buffer
 * Return: Next position in the buffer
 *
 * Wraps around the buffer when reaching the end to maintain the circular
 * nature.
 */
static int32_t next_pos(int32_t pos) { return (pos + 1) % BUF_SIZE; }

/**
 * ioq_is_full - Checks if the I/O queue is full
 * @ioq: Pointer to the ioqueue
 * Return: True if the queue is full, false otherwise
 *
 * Determines if the queue is full by comparing the next position of the head
 * with the current tail position. So the total capacity of queue is BUF_SIZE-1
 */
bool ioq_is_full(struct ioqueue *ioq) {
  ASSERT(intr_get_status() == INTR_OFF);
  return next_pos(ioq->head) == ioq->tail;
}

/**
 * ioq_is_empty - Checks if the I/O queue is empty
 * @ioq: Pointer to the ioqueue
 * Return: True if the queue is empty, false otherwise
 *
 * A queue is considered empty if the head and tail are at the same position.
 */
bool ioq_is_empty(struct ioqueue *ioq) {
  ASSERT(intr_get_status() == INTR_OFF);
  return ioq->tail == ioq->head;
}

/**
 * ioq_wait - Blocks the current thread until it can proceed
 * @waiter: Double pointer to the task_struct representing the waiting thread
 *
 * Blocks the current thread by setting its status to TASK_BLOCKED.
 */
static void ioq_wait(struct task_struct **waiter) {
  ASSERT(waiter != NULL && *waiter != NULL);
  *waiter = running_thread();
  thread_block(TASK_BLOCKED);
}

/**
 * ioq_wakeup - Unblocks a waiting thread
 * @waiter: Double pointer to the task_struct representing the thread to wake up
 *
 * Unblocks the thread pointed to by waiter and sets the pointer to NULL.
 */
static void ioq_wakeup(struct task_struct **waiter) {
  ASSERT(waiter != NULL && *waiter != NULL);
  thread_unblock(*waiter);
  *waiter = NULL;
}

/**
 * ioq_getchar - Retrieves a character from the I/O queue
 * @ioq: Pointer to the ioqueue
 * Return: The character retrieved from the queue
 *
 * Waits for the queue to be non-empty (blocking if necessary), then retrieves
 * the character from the tail and adjusts the tail position.
 */
char ioq_getchar(struct ioqueue *ioq) {
  ASSERT(intr_get_status() == INTR_OFF);
  while (ioq_is_empty(ioq)) {
    lock_acquire(&ioq->_lock);
    ioq_wait(&ioq->consumer);
    lock_release(&ioq->_lock);
  }

  char ret_char = ioq->buf[ioq->tail];
  ioq->tail = next_pos(ioq->tail);

  if (ioq->producer != NULL)
    ioq_wakeup(&ioq->producer);

  return ret_char;
}

/**
 * ioq_putchar - Puts a character into the I/O queue
 * @ioq: Pointer to the ioqueue
 * @ch: Character to put into the queue
 *
 * Waits for the queue to be non-full (blocking if necessary), then puts the
 * character at the head and adjusts the head position.
 */
void ioq_putchar(struct ioqueue *ioq, char ch) {
  ASSERT(intr_get_status() == INTR_OFF);
  while (ioq_is_full(ioq)) {
    lock_acquire(&ioq->_lock);
    ioq_wait(&ioq->consumer);
    lock_release(&ioq->_lock);
  }
  ioq->buf[ioq->head] = ch;
  ioq->head = next_pos(ioq->head);

  if (ioq->consumer != NULL)
    ioq_wakeup(&ioq->consumer);
}
