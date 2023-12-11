/*
 * Author: Xun Morris |
 * Time: 2023-11-28 |
 */
#include "console.h"
#include "fs.h"
#include "print.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "syscall.h"
#include "thread.h"
#include "fork.h"

#define syscall_nr 32
typedef void *syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid() { return running_thread()->pid; }

void syscall_init() {
  put_str("syscall_init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_FORK] =sys_fork;
  put_str("syscall_init done\n");
}
