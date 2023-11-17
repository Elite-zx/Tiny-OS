/*
 * Author: Xun Morris
 * Time: 2023-11-15
 */

#include "debug.h"
#include "init.h"
#include "print.h"

int main() {
  put_str("I am kernel\n");
  init_all();
  while (1)
    ;
  return 0;
}
