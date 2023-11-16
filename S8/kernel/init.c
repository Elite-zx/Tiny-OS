/*
 * Author: Xun Morris
 * Time: 2023-11-15
 */

#include "init.h"
#include "../device/timer.h"
#include "interrupt.h"
#include "print.h"

/**
 * initialize all modules
 */
void init_all() {
  put_str("init_all\n");
  /* initialize interrupt module  */
  idt_init();
  timer_init();
}
