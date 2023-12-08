/*
 * Author: Xun Morris |
 * Time: 2023-11-28 |
 */
#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
enum SYSCALL_NR { SYS_GETPID, SYS_WRITE };
uint32_t getpid();
uint32_t write(int32_t fd, const void *buf, uint32_t count);
#endif
