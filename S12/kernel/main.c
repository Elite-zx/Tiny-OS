/*
 * Author: Xun Morris
 * Time: 2023-11-29
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
  intr_enable();
  thread_start("kthread_a", 31, kthread_a, "I am thread_a");
  thread_start("kthread_b", 31, kthread_b, "I am thread_b");
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  void *addr = sys_malloc(33);
  console_put_str(" I am thread_a using sys_malloc(33), addr is 0x");
  console_put_int((int)addr);
  console_put_char('\n');
  while (1)
    ;
}

void kthread_b(void *arg) {
  void *addr = sys_malloc(33);
  console_put_str(" I am thread_b using sys_malloc(33), addr is 0x");
  console_put_int((int)addr);
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
