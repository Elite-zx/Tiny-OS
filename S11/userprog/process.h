/*
 * Author: Xun Morris
 * Time: 2023-11-26
 */
#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H

#include "thread.h"
#define USER_VADDR_START 0x8048000
#define default_prio 32

void process_execute(void *filename, char *name);
void process_activate(struct task_struct *pthread);
#endif
