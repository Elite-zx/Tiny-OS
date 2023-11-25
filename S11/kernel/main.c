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
#include "thread.h"

void kthread_a(void *arg);
void kthread_b(void *arg);

int main() {
  put_str("I am kernel\n");
  init_all();
  thread_start("consumer_a", 31, kthread_a, " A_");
  thread_start("consumer_b", 31, kthread_b, " B_");
  intr_enable();
  while (1)
    ;
  return 0;
}

void kthread_a(void *arg) {
  while (1) {
    enum intr_status old_status = intr_disable();
    if (!ioq_is_empty(&kbd_circular_buf)) {
      console_put_str(arg);
      char ch = ioq_getchar(&kbd_circular_buf);
      console_put_char(ch);
    }
    intr_set_status(old_status);
  }
}

void kthread_b(void *arg) {
  while (1) {
    enum intr_status old_status = intr_disable();
    if (!ioq_is_empty(&kbd_circular_buf)) {
      console_put_str(arg);
      char ch = ioq_getchar(&kbd_circular_buf);
      console_put_char(ch);
    }
    intr_set_status(old_status);
  }
}
