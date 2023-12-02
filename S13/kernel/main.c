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
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  void *addr1;
  void *addr2;
  void *addr3;
  void *addr4;
  void *addr5;
  void *addr6;
  void *addr7;
  console_put_str(" thread_a start\n");
  int max = 1000;
  while (max-- > 0) {
    int size = 128;
    addr1 = sys_malloc(size);
    size *= 2;
    addr2 = sys_malloc(size);
    size *= 2;
    addr3 = sys_malloc(size);
    sys_free(addr1);
    size *= 2;
    addr4 = sys_malloc(size);
    size *= 2;
    size *= 2;
    size *= 2;
    addr5 = sys_malloc(size);
    size *= 2;
    addr6 = sys_malloc(size);
    size *= 2;
    size *= 2;
    addr7 = sys_malloc(size);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
    sys_free(addr5);
    sys_free(addr6);
    sys_free(addr7);
  }
  console_put_str(" thread_a end\n");
  while (1)
    ;
}

void kthread_b(void *arg) {
  void *addr1;
  void *addr2;
  void *addr3;
  void *addr4;
  void *addr5;
  void *addr6;
  void *addr7;
  void *addr8;
  void *addr9;
  console_put_str(" thread_b start\n");
  int max = 1000;
  while (max-- > 0) {
    int size = 9;
    addr1 = sys_malloc(size);
    size *= 2;
    addr2 = sys_malloc(size);
    size *= 2;
    addr3 = sys_malloc(size);
    sys_free(addr1);
    size *= 2;
    addr4 = sys_malloc(size);
    size *= 2;
    size *= 2;
    size *= 2;
    addr5 = sys_malloc(size);
    size *= 2;
    addr6 = sys_malloc(size);
    size *= 2;
    size *= 2;
    addr7 = sys_malloc(size);
    size *= 2;
    size *= 2;
    addr8 = sys_malloc(size);
    size *= 2;
    size *= 2;
    addr9 = sys_malloc(size);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
    sys_free(addr5);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr8);
    sys_free(addr9);
  }
  console_put_str(" thread_b end\n");
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
