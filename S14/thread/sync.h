#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"

/**
 * struct semaphore - Defines a semaphore
 * @value: Current value of the semaphore
 * @waiters: List of threads waiting for this semaphore
 *
 * Represents a semaphore with a value indicating availability.
 * The waiters list tracks threads that are waiting on this semaphore.
 */
struct semaphore {
  uint8_t value;
  struct list waiters;
};

/**
 * struct lock - Defines a lock
 * @holder: Pointer to the task currently holding the lock
 * @sema: Semaphore controlling the lock
 * @holder_repeat_nr: Accumulated count of lock re-acquisitions by the holder
 *
 * Represents a lock mechanism using a semaphore for synchronization.
 * The holder field points to the task that currently owns the lock.
 * holder_repeat_nr is used to prevent multiple lock release by the
 * holder  (see P.449 for details).
 */
struct lock {
  struct task_struct *holder;
  struct semaphore sema;
  uint32_t holder_repeat_nr;
};

void lock_init(struct lock *plock);
void lock_acquire(struct lock *plock);
void lock_release(struct lock *plock);
void sema_init(struct semaphore *psema, uint8_t _value);
void sema_down(struct semaphore *psema);
void sema_up(struct semaphore *psema);
#endif
