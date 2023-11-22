#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "stdint.h"
void console_put_str(char *str);
void console_put_char(uint8_t ch);
void console_put_int(uint32_t num);
void console_init();
#endif
