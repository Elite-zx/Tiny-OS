/*
 * Author: Xun Morris
 * Time: 2023-11-21
 */

#include "debug.h"
#include "init.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include "thread.h"

void kthread_a(void *arg);
void kthread_b(void *arg);

int main() {
  put_str("I am kernel\n");
  init_all();
  thread_start("kthread_a", 31, kthread_a, "argA ");
  thread_start("kthread_b", 8, kthread_b, "argB ");
  intr_enable();
  while (1) {
    put_str("Main ");
  };
  return 0;
}

void kthread_a(void *arg) {
  char *para = arg;
  while (1) {
    put_str(para);
  }
}

void kthread_b(void *arg) {
  char *para = arg;
  while (1) {
    put_str(para);
  }
}
