/*
 * Author: Xun Morris
 * Time: 2023-11-21
 */
#include "console.h"
#include "debug.h"
#include "init.h"
#include "interrupt.h"
#include "io_queue.h"
#include "keyboard.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "thread.h"

void kthread_a(void *arg);
void kthread_b(void *arg);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0;
int test_var_b = 0;

int main() {
  put_str("I am kernel\n");
  init_all();
  thread_start("kthread_a", 31, kthread_a, "argA ");
  thread_start("kthread_b", 31, kthread_b, "argB ");
  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");
  intr_enable();

  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  while (1) {
    console_put_str(" v_a:0x");
    console_put_int(test_var_a);
  }
}

void kthread_b(void *arg) {
  while (1) {
    console_put_str(" v_b:0x");
    console_put_int(test_var_b);
  }
}

void u_prog_a(void) {
  while (1) {
    test_var_a++;
  }
}

void u_prog_b(void) {
  while (1) {
    test_var_b++;
  }
}
