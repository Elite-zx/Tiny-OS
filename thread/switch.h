#ifndef __THREAD_SWITCH_H
#define __THREAD_SWITCH_H
#include "thread.h"
void switch_to(struct task_struct *cur_thread, struct task_struct *next);
#endif
