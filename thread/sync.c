#include "sync.h"
#include "debug.h"
#include "interrupt.h"
#include "list.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "thread.h"

/**
 * sema_init - Initializes a semaphore
 * @psema: Pointer to the semaphore to initialize
 * @value: Initial value of the semaphore
 *
 * Initializes a semaphore with a given value and an empty waiting list.
 */
void sema_init(struct semaphore *psema, uint8_t _value) {
  psema->value = _value;
  list_init(&psema->waiters);
}

/**
 * lock_init - Initializes a lock
 * @plock: Pointer to the lock to initialize
 *
 * Sets up a lock with a null holder, resets the holder's repeat number,
 * and initializes the associated semaphore to 1 (binary semaphore).
 */
void lock_init(struct lock *plock) {
  plock->holder = NULL;
  plock->holder_repeat_nr = 0;
  /* binary semaphore with two states (0/1)  */
  sema_init(&plock->sema, 1);
}

/**
 * sema_down - Decrements the semaphore value
 * @psema: Pointer to the semaphore
 *
 * Decreases the semaphore value. If the value reaches zero, the current thread
 * is blocked and added to the semaphore's waiters list.
 */
void sema_down(struct semaphore *psema) {
  enum intr_status old_status = intr_disable();

  struct task_struct *cur_thread = running_thread();
  /** if (psema == 0) { */
  while (psema->value == 0) {
    if (list_elem_find(&psema->waiters, &cur_thread->general_tag)) {
      PANIC("The thread blocked has been in waiters list\n");
    }
    list_append(&psema->waiters, &cur_thread->general_tag);
    thread_block(TASK_BLOCKED);
  }

  psema->value--;
  ASSERT(psema->value == 0);

  intr_set_status(old_status);
}

/**
 * sema_up - Increments the semaphore value
 * @psema: Pointer to the semaphore
 *
 * Increases the semaphore value. If there are any threads blocked and waiting
 * on this semaphore, it unblocks one of them.
 */
void sema_up(struct semaphore *psema) {
  enum intr_status old_status = intr_disable();
  ASSERT(psema->value == 0);
  if (!list_empty(&psema->waiters)) {
    struct list_elem *blocked_thread_tag = list_pop(&psema->waiters);
    struct task_struct *blocked_thread =
        elem2entry(struct task_struct, general_tag, blocked_thread_tag);
    thread_unblock(blocked_thread);
  }
  psema->value++;
  ASSERT(psema->value == 1);
  intr_set_status(old_status);
}

/**
 * lock_acquire - Acquires the specified lock
 * @plock: Pointer to the lock
 *
 * Attempts to acquire the lock for the current running thread. If the lock is
 * already held by another thread, the current thread will be blocked until
 * the lock is released.
 */
void lock_acquire(struct lock *plock) {
  if (plock->holder != running_thread()) {
    sema_down(&plock->sema);
    plock->holder = running_thread();
    ASSERT(plock->holder_repeat_nr == 0);
    plock->holder_repeat_nr = 1;
  } else {
    plock->holder_repeat_nr++;
  }
}

/**
 * lock_release - Releases the specified lock
 * @plock: Pointer to the lock
 *
 * Releases the lock held by the current running thread. If the lock has been
 * acquired multiple times by the current thread, it decrements the counter
 * and only releases the lock when the counter reaches zero.
 */
void lock_release(struct lock *plock) {
  ASSERT(plock->holder == running_thread());
  if (plock->holder_repeat_nr > 1) {
    plock->holder_repeat_nr--;
    return;
  }
  ASSERT(plock->holder_repeat_nr == 1);

  plock->holder = NULL;
  plock->holder_repeat_nr = 0;
  sema_up(&plock->sema);
}
