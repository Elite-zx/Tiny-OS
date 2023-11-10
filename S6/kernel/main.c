//------------------------
// Author: Xun Morris |
// Time: 2023-11-09 |
//------------------------
#include "print.h"

int main() {
  put_str("I am kernel!\n");
  put_int(2);
  put_char('\n');
  put_int(0x0000);
  put_char('\n');
  put_int(0x0002);
  put_char('\n');
  put_int(0x0003);
  put_char('\n');
  while (1)
    ;
}
