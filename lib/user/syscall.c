/*
 * Author: Zhang Xun |
 * Time: 2023-11-28 |
 */

#include "syscall.h"
#include "console.h"
#include "dir.h"
#include "fs.h"
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

/* get the pid of current running task */
uint32_t getpid() { return _syscall0(SYS_GETPID); }

/* write data from buf to a file or standard output */
uint32_t write(int32_t fd, const void *buf, uint32_t count) {
  return _syscall3(SYS_WRITE, fd, buf, count);
}

/* copy process */
pid_t fork() { return _syscall0(SYS_FORK); }

/* read data from fd (file or standard input) to buf  */
ssize_t read(int fd, void *buf, size_t count) {
  return _syscall3(SYS_READ, fd, buf, count);
}

/* print single character on terminal  */
void putchar(char char_in_ascii) { _syscall1(SYS_PUTCHAR, char_in_ascii); }

/* clear terminal  */
void clear() { _syscall0(SYS_CLEAR); }

/* get current working directory */
char *getcwd(char *buf, uint32_t size) {
  return (char *)_syscall2(SYS_GETCWD, buf, size);
}

/* open file fd in flag way */
int32_t open(char *pathname, uint8_t flag) {
  return _syscall2(SYS_OPEN, pathname, flag);
}

/* close file fd  */
int32_t close(int32_t fd) { return _syscall1(SYS_CLOSE, fd); }

/* set the offset of file fd  */
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
  return _syscall3(SYS_LSEEK, fd, offset, whence);
}

/* delete regular file 'pathname'  */
int32_t unlink(const char *pathname) { return _syscall1(SYS_UNLINK, pathname); }

/* create directory file 'pathname' */
int32_t mkdir(const char *pathname) { return _syscall1(SYS_MKDIR, pathname); }

/* open directory file 'name' */
struct dir *opendir(const char *name) {
  return (struct dir *)_syscall1(SYS_OPENDIR, name);
}

/* close directory file 'dir' */
int32_t closedir(struct dir *dir) { return _syscall1(SYS_CLOSEDIR, dir); }

/* close directory file 'pathname' */
int32_t rmdir(const char *pathname) { return _syscall1(SYS_RMDIR, pathname); }

/* read directory entry in 'dir'  */
struct dir_entry *readdir(struct dir *dir) {
  return (struct dir_entry *)_syscall1(SYS_READDIR, dir);
}

/* reset directory pointer (dir_pos in 'dir')  */
void rewinddir(struct dir *dir) { _syscall1(SYS_REWINDDIR, dir); }

/* get the attributes of 'path' to 'buf'  */
int32_t stat(const char *path, struct stat *buf) {
  return _syscall2(SYS_STAT, path, buf);
}

/* change cwd to 'path'  */
int32_t chdir(const char *path) { return _syscall1(SYS_CHDIR, path); }

/* print task list  */
void ps(void) { _syscall0(SYS_PS); }
