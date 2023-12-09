/*
 * Author: Xun Morris
 * Time: 2023-11-29
 */
#include "console.h"
#include "debug.h"
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
  process_execute(u_prog_a, "u_prog_a");
  process_execute(u_prog_b, "u_prog_b");
  thread_start("kthread_a", 31, kthread_a, "I am thread a");
  thread_start("kthread_b", 31, kthread_b, "I am thread b");

  /** uint32_t fd = sys_open("/file1", O_CREAT); */
  /** uint32_t fd = sys_open("/file1", O_RDONLY | O_RDWR); */
  /** sys_write(fd, "hello,world\n", 12); */
  printf("/file1 delete %s!\n", sys_unlink("/file1") == 0 ? "done" : "fail");

  /** sys_close(fd); */
  /** printk("%d close!\n", fd); */
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  while (1)
    ;
}

void kthread_b(void *arg) {
  while (1)
    ;
}

void u_prog_a(void) {
  while (1)
    ;
}

void u_prog_b(void) {
  while (1)
    ;
}
