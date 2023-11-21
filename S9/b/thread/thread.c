/*
 * Author: Xun Morris
 * Time: 2023-11-20
 */
#include "thread.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "print.h"
#include "stdint.h"
#include "string.h"
#include "switch.h"

#define PG_SIZE 1024

struct task_struct *main_thread;
struct list thread_ready_list;
struct list thread_all_list;

/* Get the PCB of the current thread  */
struct task_struct *running_thread() {
  uint32_t esp;
  asm volatile("mov %%esp,%0" : "=g"(esp));
  return (struct task_struct *)(esp & 0xfffff000);
}

/* turn off interrupt, execute function(func_arg)  */
static void kernel_thread(thread_func *function, void *func_arg) {
  intr_enable();
  function(func_arg);
}

/**
 * thread_create - Initializes a thread stack.
 * @thread: Pointer to the task_struct representing the thread.
 * @function: The function to be executed by the thread.
 * @func_arg: Argument to be passed to the thread function.
 *
 * This function sets up the thread stack for a new thread, placing the function
 * and its argument in the appropriate position in the thread_stack. It also
 * prepares the stack to start execution at `kernel_thread`.
 */
void thread_create(struct task_struct *thread, thread_func function,
                   void *func_arg) {
  thread->self_kstack -= sizeof(struct intr_stack);
  thread->self_kstack -= sizeof(struct thread_stack);
  struct thread_stack *kthread_stack =
      (struct thread_stack *)thread->self_kstack;

  kthread_stack->ebp = kthread_stack->ebx = 0;
  kthread_stack->esi = kthread_stack->edi = 0;

  kthread_stack->eip = kernel_thread;
  kthread_stack->function = function;
  kthread_stack->func_arg = func_arg;
}

void init_thread(struct task_struct *thread, char *name, int _priority) {
  /* clear pcb to 0  */
  memset(thread, 0, sizeof(*thread));
  strcpy(thread->name, name);
  if (thread == main_thread) {
    /* The main thread is already running. The main thread is initialized here
     * to make up a PCB for it. */
    thread->status = TASK_RUNNING;
  } else {
    thread->status = TASK_READY;
  }
  /* Let the stack pointer point to the high address */
  thread->self_kstack = (uint32_t *)((uint32_t)thread + PG_SIZE);
  thread->priority = _priority;
  /* the larger priority is, the longer time slice is */
  thread->ticks = _priority;
  thread->elapsed_ticks = 0;
  thread->pg_dir = NULL;
  thread->stack_magic = 0x20011124;
}

struct task_struct *thread_start(char *name, int _priority,
                                 thread_func function, void *func_arg) {
  struct task_struct *thread = get_kernel_pages(1);
  init_thread(thread, name, _priority);
  thread_create(thread, function, func_arg);

  ASSERT(!list_elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);
  ASSERT(!list_elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);

  return thread;
}

static void make_main_thread() {
  main_thread = running_thread();
  init_thread(main_thread, "main", 36);

  ASSERT(!list_elem_find(&thread_all_list, &main_thread->all_list_tag));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

void schedule() {
  ASSERT(intr_get_status() == INTR_OFF);
  struct task_struct *cur_thread = running_thread();
  if (cur_thread->status == TASK_RUNNING) {
    /* the time slice for current thread is used up  */

    /* make sure cur_thread is not in thread_ready_list  */
    ASSERT(!list_elem_find(&thread_ready_list, &cur_thread->general_tag));

    list_append(&thread_ready_list, &cur_thread->general_tag);
    cur_thread->ticks = cur_thread->priority;
    cur_thread->status = TASK_READY;
  } else {
    /* other events  */
  }

  ASSERT(!list_empty(&thread_ready_list));
  struct list_elem *thread_tag;
  thread_tag = list_pop(&thread_ready_list);
  struct task_struct *next =
      elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;
  switch_to(cur_thread, next);
}

void thread_init() {
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  make_main_thread();
  put_str("thread_init done\n");
}
