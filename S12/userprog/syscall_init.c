#include "print.h"
#include "stdint.h"
#include "syscall.h"
#include "thread.h"

#define syscall_nr 32
typedef void *syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid() { return running_thread()->pid; }

void syscall_init() {
  put_str("syscall_init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  put_str("syscall_init done\n");
}
