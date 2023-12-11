/*
 * Author: Xun Morris
 * Time: 2023-11-15
 */

#include "debug.h"
#include "interrupt.h"
#include "print.h"

/**
 * panic_spin - Print an error message and abort the current program
 * @filename: the file where the error occured
 * @line: the line number where the error occured
 * @func: the function name where the error occured
 * @func: conditional statement in assert
 *
 * This function is called in macro assert when the expression in macro assert
 * is false.
 *
 */
void panic_spin(char *filename, int line, const char *func,
                const char *condition) {
  intr_disable();
  put_str("\n\n\n!!!!!!error!!!!!!\n");

  put_str("filename: ");
  put_str(filename);
  put_str("\n");

  put_str("line: 0x");
  put_int(line);
  put_str("\n");

  put_str("function: ");
  put_str((char *)func);
  put_str("\n");

  put_str("condition: ");
  put_str((char *)condition);
  put_str("\n");
  while (1);
}
