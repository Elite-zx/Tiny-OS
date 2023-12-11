/*
 * Author: Xun Morris |
 * Time: 2023-11-28 |
 */
#include "console.h"
#include "fork.h"
#include "fs.h"
#include "print.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "syscall.h"
#include "thread.h"

#define syscall_nr 32
typedef void *syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid() { return running_thread()->pid; }
void sys_putchar(char char_in_ascii) { console_put_char(char_in_ascii); }

void syscall_init() {
  put_str("syscall_init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_FORK] = sys_fork;
  syscall_table[SYS_READ] = sys_read;
  syscall_table[SYS_PUTCHAR] = sys_putchar;
  syscall_table[SYS_CLEAR] = sys_clear;
  put_str("syscall_init done\n");
}
