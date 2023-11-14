/*
 * Author: Xun Morris
 * Time: 2023-11-13
 */

#include "init.h"
#include "print.h"

int main() {
  put_str("I am kernel\n");
  init_all();
  /* Turn on the bit IF and let the CPU respond to interrupts from chip 8259A */
  asm volatile("sti");
  while (1) {
  };
}
