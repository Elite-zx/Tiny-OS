/*
 * Author: Xun Morris
 * Time: 2023-11-13
 */

#include "init.h"
#include "../device/timer.h"
#include "interrupt.h"
#include "print.h"

/**
 * init_all - initialize all modules
 *
 * This function calls the Initialization function of various modules
 */
void init_all() {
  put_str("init_all\n");
  /* initialize interrupt module  */
  idt_init();
  timer_init();
}
