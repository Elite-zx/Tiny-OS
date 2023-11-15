/*
 * Author: Xun Morris
 * Time: 2023-11-13
 */

#include "debug.h"
#include "init.h"
#include "print.h"

int main() {
  put_str("I am kernel\n");
  init_all();
  ASSERT(1 == 2);
  while (1)
    ;
  return 0;
}
