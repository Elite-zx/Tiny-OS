/*
 * Author: Zhang Xun
 * Time: 2023-11-25
 */
#ifndef __USERPROG_TSS_H
#define __USERPROG_TSS_H
#include "thread.h"
void tss_init();
void update_tss_esp(struct task_struct *pthread);
#endif
