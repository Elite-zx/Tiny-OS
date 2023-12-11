/*
 * Author: Zhang Xun
 * Time: 2023-11-28
 */
#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
#include "stdint.h"
typedef char *va_list;

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

uint32_t printf(const char *format, ...);
uint32_t vsprintf(char *str, const char *format, va_list ap);
uint32_t sprintf(char *buf, const char *format, ...);

#endif
