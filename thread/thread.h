/*
 * Author: Zhang Xun
 * Time: 2023-11-29
 */
#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "list.h"
#include "memory.h"
#include "stdint.h"

#define MAX_FILES_OPEN_PER_PROC 8
#define TASK_NAME_LEN 16

typedef void thread_func(void *);
typedef int16_t pid_t;

/**
 * enum task_status - Enumerates different thread states.
 *
 * Enumerates the various states that a thread can be in during its lifecycle.
 */
enum task_status {
  TASK_RUNNING,
  TASK_READY,
  TASK_BLOCKED,
  TASK_WAITING,
  TASK_HANGING,
  TASK_DIED
};

/**
 * struct intr_stack - Represents a stack used during an interrupt.
 * @vec_no: Vector number of the interrupt.
 * @edi, esi, ebp, ebx, edx, ecx, eax: Saved registers.
 * @esp_dummy: Dummy stack pointer (not used).
 * @gs, fs, es, ds: Segment registers.
 * @error_code: Error code of interrupt.
 * @eip: Instruction pointer.
 * @cs: Code segment.
 * @eflags: CPU flags.
 * @esp: Stack pointer.
 * @ss: Stack segment.
 *
 * This structure represents the stack state during an interrupt and
 * is used to save the context of the thread that was interrupted.
 */
struct intr_stack {
  uint32_t vec_no;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;

  uint32_t error_code;
  void (*eip)(void);
  uint32_t cs;
  uint32_t eflags;
  void *esp;
  uint32_t ss;
};

/**
 * struct thread_stack - Represents a thread's stack for a function execution.
 * @ebp, ebx, edi, esi: Saved registers for the caller function.
 * @eip: Instruction pointer.
 * @unused_retaddr: Unused return address placeholder.
 * @function: Function to be executed by the thread.
 * @func_arg: Argument to the function.
 *
 * This structure represents the stack set up for a thread that is about to
 * start executing a given function.
 */
struct thread_stack {
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;

  void (*eip)(thread_func *func, void *func_arg);

  void(*unused_retaddr);
  thread_func *function;
  void *func_arg;
};

struct task_struct {
  uint32_t *self_kstack;
  pid_t pid;
  enum task_status status;
  uint8_t priority;
  char name[TASK_NAME_LEN];

  uint8_t ticks;
  uint32_t elapsed_ticks;

  uint32_t fd_table[MAX_FILES_OPEN_PER_PROC];

  struct list_elem general_tag;
  struct list_elem all_list_tag;

  /* page table, NULL if it is TCB, */
  uint32_t *pg_dir;

  /* virtual memory pool of user process */
  struct virtual_addr userprog_vaddr;
  struct mem_block_desc u_mb_desc_arr[MB_DESC_CNT];

  /* the inode number of current working directory   */
  uint32_t cwd_inode_NO;

  int16_t parent_pid;

  uint32_t stack_magic;
};

struct task_struct *thread_start(char *name, int _priority,
                                 thread_func function, void *func_arg);
void thread_init();
struct task_struct *running_thread();
void schedule();
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct *pthread);
void init_thread(struct task_struct *thread, char *name, int _priority);
void thread_create(struct task_struct *thread, thread_func function,
                   void *func_arg);
void thread_yield();
pid_t fork_pid(void);
void sys_ps();
#endif
