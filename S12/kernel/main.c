/*
 * Author: Xun Morris
 * Time: 2023-11-28
 */
#include "console.h"
#include "debug.h"
#include "init.h"
#include "interrupt.h"
#include "io_queue.h"
#include "keyboard.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stdio.h"
#include "syscall.h"
#include "syscall_init.h"
#include "thread.h"

void kthread_a(void *arg);
void kthread_b(void *arg);
void u_prog_a(void);
void u_prog_b(void);
int prog_a_pid = 0;
int prog_b_pid = 0;

int main() {
  put_str("I am kernel\n");
  init_all();
  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");
  intr_enable();

  console_put_str(" I am main with pid: 0x");
  console_put_int(sys_getpid());
  console_put_char('\n');

  thread_start("kthread_a", 31, kthread_a, "argA ");
  thread_start("kthread_b", 31, kthread_b, "argB ");

  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  console_put_str(" I am thread_a with pid: 0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  while (1)
    ;
}

void kthread_b(void *arg) {
  console_put_str(" I am thread_b with pid: 0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  while (1)
    ;
}

void u_prog_a(void) {
  char *name = "u_prog_a";
  printf(" I am %s with pid: %d%c", name, getpid(), '\n');
  while (1)
    ;
}

void u_prog_b(void) {
  char *name = "u_prog_b";
  printf(" I am %s with pid: %d%c", name, getpid(), '\n');
  while (1)
    ;
}
