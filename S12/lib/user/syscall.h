/*
 * Author: Xun Morris |
 * Time: 2023-11-28 |
 */
#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "stdint.h"
enum SYSCALL_NR { SYS_GETPID, SYS_WRITE, SYS_MALLOC, SYS_FREE };

uint32_t getpid();
uint32_t write(char *str);
void *malloc(uint32_t size);
void free(uint32_t *ptr);

#endif
