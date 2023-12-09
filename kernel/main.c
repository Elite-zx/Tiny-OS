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
#include "syscall.h"
#include "syscall_init.h"
#include "thread.h"
#include <string.h>

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

  uint32_t fd = sys_open("/file1", O_CREAT);
  /** printf("open /file1, fd:%d\n", fd); */
  /** char buf[64] = {0}; */
  /** int read_bytes = sys_read(fd, buf, 18); */
  /** printf("1 read %d bytes:\n%s\n", read_bytes, buf); */
  /**  */
  /** memset(buf, 0, 64); */
  /** read_bytes = sys_read(fd, buf, 6); */
  /** printf("2 read %d bytes:\n%s", read_bytes, buf); */
  /**  */
  /** memset(buf, 0, 64); */
  /** read_bytes = sys_read(fd, buf, 6); */
  /** printf("3 read %d bytes:\n%s", read_bytes, buf); */
  /**  */
  /** printf("____________ SEEK_SET 0 ____________\n"); */
  /** sys_lseek(fd, 0, SEEK_SET); */
  /** memset(buf, 0, 64); */
  /** read_bytes = sys_read(fd, buf, 24); */
  /** printf("4 read %d bytes:\n%s", read_bytes, buf); */

  sys_close(fd);
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
