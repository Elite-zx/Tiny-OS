/*
 * Author: Zhang Xun |
 * Time: 2023-11-28 |
 */
#include "console.h"
#include "exec.h"
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
  syscall_table[SYS_GETCWD] = sys_getcwd;
  syscall_table[SYS_OPEN] = sys_open;
  syscall_table[SYS_CLOSE] = sys_close;
  syscall_table[SYS_LSEEK] = sys_lseek;
  syscall_table[SYS_UNLINK] = sys_unlink;
  syscall_table[SYS_MKDIR] = sys_mkdir;
  syscall_table[SYS_OPENDIR] = sys_opendir;
  syscall_table[SYS_CLOSEDIR] = sys_closedir;
  syscall_table[SYS_CHDIR] = sys_chdir;
  syscall_table[SYS_RMDIR] = sys_rmdir;
  syscall_table[SYS_READDIR] = sys_readdir;
  syscall_table[SYS_REWINDDIR] = sys_rewinddir;
  syscall_table[SYS_STAT] = sys_stat;
  syscall_table[SYS_PS] = sys_ps;
  syscall_table[SYS_EXECV] = sys_execv;
  put_str("syscall_init done\n");
}
