#include "stdio_kernel.h"
#include "console.h"
#include "global.h"
#include "stdio.h"

void printk(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  console_put_str(buf);
}
