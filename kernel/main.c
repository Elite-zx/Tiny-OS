/*
 * Author: Xun Morris
 * Time: 2023-11-29
 */
#include "console.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "io_queue.h"
#include "keyboard.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stdint.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "string.h"
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
  while (1);
  return 0;
}

void kthread_a(void *arg) {
  while (1);
}

void kthread_b(void *arg) {
  while (1);
}

void u_prog_a(void) {
  while (1);
}

void u_prog_b(void) {
  while (1);
}

void init()
{
  uint32_t ret_pid = fork();
  if(ret_pid)
  {
    printf("I am father, my pid is %d, child pid is %d\n",getpid(),ret_pid);
  }
  else {
    printf("I am child, my pid is %d, ret pid is %d\n",getpid(),ret_pid);
  }
  while (1);
}
