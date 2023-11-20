#include "thread.h"
#include "global.h"
#include "memory.h"
#include "stdint.h"
#include "string.h"

#define PG_SIZE 1024

/**
 * kernel_thread - Wrapper function for a thread function.
 * @function: The function to be executed by the thread.
 * @func_arg: The argument to be passed to the thread function.
 *
 * This function serves as a wrapper for the actual thread function. It is
 * designed to execute the thread function with its argument.
 */
static void kernel_thread(thread_func *function, void *func_arg) {
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

/**
 * init_thread - Initializes basic information of a thread.
 * @thread: Pointer to the task_struct to be initialized.
 * @name: Name of the thread.
 * @priority: Priority of the thread.
 *
 * Initializes the task_struct representing a thread with the given name and
 * priority. It sets the initial status, stack, and other basic properties of
 * the thread.
 */
void init_thread(struct task_struct *thread, char *name, int _priority) {
  memset(thread, 0, sizeof(*thread));
  strcpy(thread->name, name);
  thread->status = TASK_RUNNING;
  thread->priority = _priority;
  thread->self_kstack = (uint32_t *)((uint32_t)thread + PG_SIZE);
  thread->stack_magic = 0x20011124;
}

/**
 * thread_start - Creates and starts a new thread.
 * @name: Name of the new thread.
 * @priority: Priority of the new thread.
 * @function: Function to be executed by the new thread.
 * @func_arg: Argument for the thread function.
 *
 * Allocates a task_struct for the new thread and initializes it using
 * `init_thread` and `thread_create`. It sets up the environment for the new
 * thread to start executing `kernel_thread`. The assembly block at the end
 * manipulates the stack to return to function `kernel_thread`.
 *
 * Return: Pointer to the newly created task_struct.
 */
struct task_struct *thread_start(char *name, int _priority,
                                 thread_func function, void *func_arg) {
  struct task_struct *thread = get_kernel_pages(1);
  init_thread(thread, name, _priority);
  thread_create(thread, function, func_arg);

  /* treat the address of function kernel_thread as the return address of
   * thread_start, so kernel_thread can be executed by the ret instruction.  */
  asm volatile("movl %0,%%esp; pop %%ebp; pop %%ebx; \
                pop %%edi; pop %%esi; ret" ::"g"(thread->self_kstack)
               : "memory");
  return thread;
}
