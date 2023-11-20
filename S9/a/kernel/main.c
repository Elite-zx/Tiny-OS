/*
 * Author: Xun Morris
 * Time: 2023-11-15
 */

#include "debug.h"
#include "init.h"
#include "memory.h"
#include "print.h"
#include "thread.h"

void kthread_a(void *arg);

int main() {
  put_str("I am kernel\n");
  init_all();
  thread_start("kthread_a", 3, kthread_a, "argA ");
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  char *para = arg;
  while (1) {
    put_str(para);
  }
}
