/*
 * Author: Zhang Xun
 * Time: 2023-11-15
 */

#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char *filename, int line, const char *func,
                const char *condition);

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * ASSERT(CONDITION) - a function-like macro
 * @CONDITION: An expression that can be converted to a Boolean type
 *
 * This function-like macro is used to debug the program, triggering the
 * panic_spin function when the expression is false, otherwise it does nothing.
 * If NDEBUG is defined in the current file, assert becomes meaningless (does
 * nothing).
 *
 */
#ifdef NDEBUG
#define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION)                                                      \
  if (CONDITION) {                                                             \
  } else {                                                                     \
    PANIC(#CONDITION);                                                         \
  }
#endif /*__NDEBUG*/

// #define assert ASSERT

#endif /*__KERNEL_DEBUG_H*/
