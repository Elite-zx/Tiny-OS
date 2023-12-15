/*
 * Author: Zhang Xun
 * Time: 2023-11-29
 */
#include "console.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "global.h"
#include "ide.h"
#include "init.h"
#include "interrupt.h"
#include "io_queue.h"
#include "keyboard.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "shell.h"
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

extern struct ide_channel channels[2];

int main() {
  put_str("I am kernel\n");
  init_all();

  uint32_t file_size = 22624;
  uint32_t sector_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);

  /* hd60M.img  */
  struct disk *sda = &channels[0].devices[0];

  /* read the user process into prog_buf  */
  void *prog_buf = sys_malloc(file_size);
  ide_read(sda, 300, prog_buf, sector_cnt);

  int32_t fd = sys_open("/prog_no_arg", O_CREAT | O_RDWR);
  if (fd != -1) {
    /* Write the user process to the file prog_no_arg (locate in hd80M.img) */
    if (sys_write(fd, prog_buf, file_size) == -1) {
      printk("file write error!\n");
      while (1)
        ;
    }
  }

  sys_clear();
  console_put_str("[Pench@localhost /]$ ");
  intr_enable();
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

void init() {
  uint32_t ret_pid = fork();
  if (ret_pid) {
    while (1)
      ;
  } else {
    zx_shell();
  }
  PANIC("init: something wrong!");
}
