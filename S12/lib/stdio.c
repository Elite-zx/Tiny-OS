#include "stdio.h"
#include "global.h"
#include "stdint.h"
#include "string.h"
#include "syscall.h"

/* va_start - Initialize a va_list for variable argument list processing.
 * @ap: A va_list to be initialized.
 * @v: The last fixed argument before the variable list starts.
 *
 * This macro initializes ap to point to the first argument in a variadic
 * function's argument list, which allows subsequent access via va_arg and
 * va_end.
 */
#define va_start(ap, v) ap = (va_list)&v

/* va_arg - Retrieve the next argument from a va_list.
 * @ap: A va_list from which to retrieve the argument.
 * @t: The type of the next argument to be retrieved.
 *
 * This macro accesses the next argument in a va_list and increments ap
 * to point to the following argument. The type of the argument is specified
 * by t.
 */
#define va_arg(ap, t) *((t *)(ap += 4))

/* va_end - Cleans up a va_list after processing.
 * @ap: The va_list to be cleaned up.
 *
 * This macro performs necessary cleanup after processing a variadic argument
 * list. It should be called before the function returns.
 */
#define va_end(ap) ap = NULL

/* itoa - Convert an integer value to a string based on the specified base.
 * @value: Integer value to be converted.
 * @buf_ptr_addr: Pointer to a buffer where the converted string will be stored.
 * @base: Base for conversion (e.g., 10 for decimal, 16 for hexadecimal).
 *
 * Converts the given integer (value) into a string representation in the
 * specified base and stores it in the provided buffer.
 */
static void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base) {
  uint32_t m = value % base;
  uint32_t i = value / base;
  if (i) {
    itoa(i, buf_ptr_addr, base);
  }
  if (m < 10) {
    *((*buf_ptr_addr)++) = m + '0';
  } else {
    *((*buf_ptr_addr)++) = m - 10 + 'A';
  }
}

/* vsprintf - Formats a string and stores it in the provided buffer.
 * @str: Buffer to store the formatted string.
 * @format: Format string specifying the desired output.
 * @ap: Variable argument list providing values to format.
 *
 * This function formats a string according to the specified format and argument
 * list and stores the result in str. It's used internally by printf and similar
 * functions. Returns the length of the formatted string.
 */
uint32_t vsprintf(char *str, const char *format, va_list ap) {
  char *buf_ptr = str;
  const char *iter = format;
  char ch = *iter;
  int32_t arg_int;
  char *arg_str;
  while (ch) {
    if (ch != '%') {
      *(buf_ptr++) = ch;
      ch = *(++iter);
      continue;
    }
    /* Get the character after the character %, which represents the base */
    ch = *(++iter);
    switch (ch) {
    case 'x':
      arg_int = va_arg(ap, int);
      itoa(arg_int, &buf_ptr, 16);
      ch = *(++iter);
      break;
    case 'c':
      *(buf_ptr++) = va_arg(ap, char);
      ch = *(++iter);
      break;
    case 'd':
      arg_int = va_arg(ap, int);
      if (arg_int < 0) {
        arg_int = 0 - arg_int;
        *(buf_ptr++) = '-';
      }
      itoa(arg_int, &buf_ptr, 10);
      ch = *(++iter);
      break;
    case 's':
      arg_str = va_arg(ap, char *);
      strcpy(buf_ptr, arg_str);
      buf_ptr += strlen(arg_str);
      ch = *(++iter);
      break;
    }
  }
  return strlen(str);
}

uint32_t sprintf(char *buf, const char *format, ...) {
  va_list args;
  uint32_t ret_val;
  va_start(args, format);
  ret_val = vsprintf(buf, format, args);
  va_end(args);
  return ret_val;
}
/* printf - Formats and prints a string to the standard output.
 * @format: Format string specifying the desired output.
 *
 * This function takes a format string and a variable number of arguments,
 * formats them into a string, and prints the string to the standard output.
 * Returns the number of characters printed.
 */
uint32_t printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  return write(buf);
}
