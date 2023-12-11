/*
 * Author: Xun Morris |
 * Time: 2023-11-28 |
 */

#include "syscall.h"
#include "console.h"
#include "print.h"
#include "stdint.h"
#include "thread.h"

#define _syscall0(NUMBER)                                                      \
  ({                                                                           \
    int retval;                                                                \
    asm volatile("int $0x80" : "=a"(retval) : "a"(NUMBER) : "memory");         \
    retval;                                                                    \
  })

#define _syscall1(NUMBER, ARG1)                                                \
  ({                                                                           \
    int retval;                                                                \
    asm volatile("int $0x80"                                                   \
                 : "=a"(retval)                                                \
                 : "a"(NUMBER), "b"(ARG1)                                      \
                 : "memory");                                                  \
    retval;                                                                    \
  })

#define _syscall2(NUMBER, ARG1, ARG2)                                          \
  ({                                                                           \
    int retval;                                                                \
    asm volatile("int $0x80"                                                   \
                 : "=a"(retval)                                                \
                 : "a"(NUMBER), "b"(ARG1), "c"(ARG2)                           \
                 : "memory");                                                  \
    retval;                                                                    \
  })

#define _syscall3(NUMBER, ARG1, ARG2, ARG3)                                    \
  ({                                                                           \
    int retval;                                                                \
    asm volatile("int $0x80"                                                   \
                 : "=a"(retval)                                                \
                 : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3)                \
                 : "memory");                                                  \
    retval;                                                                    \
  })

uint32_t getpid() { return _syscall0(SYS_GETPID); }

uint32_t write(int32_t fd, const void *buf, uint32_t count) {
  return _syscall3(SYS_WRITE, fd, buf, count);
}

pid_t fork() { return _syscall0(SYS_FORK); }

ssize_t read(int fd, void *buf, size_t count) {
  return _syscall3(SYS_READ, fd, buf, count);
}

void putchar(char char_in_ascii) { _syscall1(SYS_PUTCHAR, char_in_ascii); }

void clear() { _syscall0(SYS_CLEAR); }
