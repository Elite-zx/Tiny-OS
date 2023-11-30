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
  process_execute(u_prog_a, "u_prog_a");
  process_execute(u_prog_b, "u_prog_b");
  thread_start("kthread_a", 31, kthread_a, "I am thread_a");
  thread_start("kthread_b", 31, kthread_b, "I am thread_b");
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  void *addr1 = sys_malloc(256);
  void *addr2 = sys_malloc(256);
  void *addr3 = sys_malloc(256);
  console_put_str(" thread_a malloc addr: 0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');

  int cpu_sleep = 100000;
  while (cpu_sleep-- > 0)
    ;
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while (1)
    ;
}

void kthread_b(void *arg) {
  void *addr1 = sys_malloc(256);
  void *addr2 = sys_malloc(256);
  void *addr3 = sys_malloc(256);
  console_put_str(" thread_b malloc addr: 0x");
  console_put_int((int)addr1);
  console_put_char(',');
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');

  int cpu_sleep = 100000;
  while (cpu_sleep-- > 0)
    ;
  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);
  while (1)
    ;
}

void u_prog_a(void) {
  void *addr1 = malloc(256);
  void *addr2 = malloc(256);
  void *addr3 = malloc(256);
  printf(" prog_a malloc addr: 0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2,
         (int)addr3);

  int cpu_sleep = 100000;
  while (cpu_sleep-- > 0)
    ;
  free(addr1);
  free(addr2);
  free(addr3);
  while (1)
    ;
}

void u_prog_b(void) {
  void *addr1 = malloc(256);
  void *addr2 = malloc(256);
  void *addr3 = malloc(256);
  printf(" prog_b malloc addr: 0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2,
         (int)addr3);

  int cpu_sleep = 100000;
  while (cpu_sleep-- > 0)
    ;
  free(addr1);
  free(addr2);
  free(addr3);
  while (1)
    ;
}
