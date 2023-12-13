#ifndef __LIB_USER_assert_H
#define __LIB_USER_assert_H

void user_spin(char *filename, int line, const char *func,
               const char *condition);

#define panic(...) user_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * assert(CONDITION) - a function-like macro
 * @CONDITION: An expression that can be converted to a Boolean type
 *
 * This function-like macro is used to debug the program, triggering the
 * panic_spin function when the expression is false, otherwise it does nothing.
 * If NDEBUG is defined in the current file, assert becomes meaningless (does
 * nothing).
 *
 */
#ifdef NDEBUG
#define assert(CONDITION) ((void)0)
#else
#define assert(CONDITION)                                                      \
  if (CONDITION) {                                                             \
  } else {                                                                     \
    panic(#CONDITION);                                                         \
  }
#endif /*__NDEBUG*/
#endif
