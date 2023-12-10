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
  process_execute(u_prog_a, "u_prog_a");
  process_execute(u_prog_b, "u_prog_b");
  thread_start("kthread_a", 31, kthread_a, "I am thread a");
  thread_start("kthread_b", 31, kthread_b, "I am thread b");

  struct stat obj_stat;
  sys_stat("/", &obj_stat);
  printf("/'s info\n  i_no:%d\n  size:%d\n  file_type:%s\n", obj_stat.st_ino,
         obj_stat.st_size,
         obj_stat.st_filetype == FT_DIRECTORY ? "directory" : "regular");
  sys_stat("/dir1", &obj_stat);
  printf("/dir1's info\n  i_no:%d\n  size:%d\n  file_type:%s\n  ",
         obj_stat.st_ino, obj_stat.st_size,
         obj_stat.st_filetype == FT_DIRECTORY ? "directory" : "regular");
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
