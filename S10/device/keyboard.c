#include "keyboard.h"
#include "interrupt.h"
#include "io.h"
#include "print.h"
#include <stdint.h>

/* Output port number of keyboard controller (8024chip)  */
#define KBD_BUF_PORT 0x60

static void intr_keyboard_handler(void) {
  uint8_t scan_code = inb(KBD_BUF_PORT);
  put_int(scan_code);
  return;
}

void keyboard_init() {
  put_str("keyboard init start\n");
  register_handler(0x21, intr_keyboard_handler);
  put_str("keyboard init done\n");
}
