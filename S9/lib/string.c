/*
 * Author: Xun Morris
 * Time: 2023-11-16
 */

#include "string.h"
#include "debug.h"
#include "global.h"

/**
 * memset - set size bytes starting from dst to value
 * @dst: a pointer to starting address
 * @value: value to set
 * @size: the number of bytes to set
 *
 * This function is unsafe. size cannot exceed the range that dst
 * can access, Otherwise it will lead to illegal memory access.
 */
void memset(void *dst, uint8_t value, uint32_t size) {
  ASSERT(dst != NULL);
  uint8_t *dst_in_byte = (uint8_t *)dst;
  while (size-- > 0)
    *dst_in_byte++ = value;
}

/**
 * memcpy - Copies size bytes from the address pointed to by src to the address
 * pointed to by dest
 * @dst: destination of copy
 * @src: source of copy
 * @size: the number of bytes to copy
 *
 * This function is unsafe, If the length of src is greater than dst, it will
 * result in illegal memory access.
 */
void memcpy(void *dst, const void *src, uint32_t size) {
  ASSERT(dst != NULL && src != NULL);
  uint8_t *dst_in_byte = (uint8_t *)dst;
  const uint8_t *src_in_byte = (uint8_t *)src;
  while (size-- > 0) {
    *dst_in_byte++ = *src_in_byte++;
  }
}

/**
 * memcmp - Compares the first size bytes of the block of memory pointed by a to
 * the first size bytes pointed by b
 * @a: a pointer to the starting address of first memory block
 * @b: a pointer to the starting address of second memory block
 *
 * Return: returning zero if they all match or a value different from zero
 * representing which is greater if they do not.
 */
int memcmp(const void *a, const void *b, unsigned long size) {
  ASSERT(a != NULL && b != NULL);
  const char *a_in_char = a;
  const char *b_in_char = b;
  while (size-- > 0) {
    if (*a_in_char != *b_in_char)
      return *a_in_char > *b_in_char ? 1 : -1;
    ++a_in_char;
    ++b_in_char;
  }
  return 0;
}

/**
 * strcpy - Copies the C string pointed by src into the array pointed by dst,
 * including the terminating null character
 * @dst: a pointer to the destination array where the content is to be copied
 * @src: a pointer to the null-terminated byte string to be copied
 *
 * Return: a pointer to the destination string dst.
 */
char *strcpy(char *dst, const char *src) {
  ASSERT(dst != NULL && src != NULL);
  char *ret = dst;
  while ((*dst++ = *src++))
    ;
  return ret;
}

/**
 * strlen - Calculates the length of the string pointed to by str,
 * excluding the terminating null byte ('\0')
 * @str: a pointer to the null-terminated byte string whose length is to be
 * measured
 *
 * Return: the number of characters in the string pointed to by str.
 */
uint32_t strlen(const char *str) {
  ASSERT(str != NULL);
  const char *p = str;
  while (*p++)
    ;
  return p - str - 1;
}

/**
 * strcmp - Compares the two strings pointed to by a and b
 * @a: a pointer to the first null-terminated byte string to compare
 * @b: a pointer to the second null-terminated byte string to compare
 *
 * Return: an integer less than, equal to, or greater than zero if the string
 * pointed to by a is found, respectively, to be less than, to match, or be
 * greater than the string pointed to by b.
 */
int8_t strcmp(const char *a, const char *b) {
  ASSERT(a != NULL && b != NULL);
  while (*a != 0 && *a == *b) {
    ++a;
    ++b;
  }
  return *a < *b ? -1 : *a > *b;
}

/**
 * strcat - Appends the string pointed to by src to the end of the string
 * pointed to by dst. It overwrites the null byte ('\0') at the end of dst,
 * and then adds a terminating null byte
 * @dst: a pointer to the destination array, which should contain a C string,
 * and be large enough to contain the concatenated resulting string
 * @src: a pointer to the null-terminated byte string to be appended
 *
 * Return: a pointer to the resulting string dst.
 */
char *strcat(char *dst, const char *src) {
  ASSERT(dst != NULL && src != NULL);

  char *iter_dst = dst;
  while (*iter_dst++)
    ;
  --iter_dst;

  while ((*iter_dst++ = *src++))
    ;
  return dst;
}

/**
 * strchr - Locates the first occurrence of ch (converted to a char) in the
 * string pointed to by str
 * @str: a pointer to the null-terminated byte string to be analyzed
 * @ch: the character to search for
 *
 * Return: a pointer to the first occurrence of the character ch in the string
 * str, or NULL if the character is not found.
 */
char *strchr(const char *str, const uint8_t ch) {
  ASSERT(str != NULL);
  while (*str != 0 && *str != ch)
    ++str;
  return *str == ch ? (char *)str : NULL;
}

/**
 * strrchr - Locates the last occurrence of ch (converted to a char) in the
 * string pointed to by str
 * @str: a pointer to the null-terminated byte string to be scanned
 * @ch: the character to search for
 *
 * Return: a pointer to the last occurrence of the character ch in the string
 * str, or NULL if the character is not found.
 */
char *strrchr(const char *str, int ch) {
  ASSERT(str != NULL);
  const char *last_ch = NULL;
  while (*str != 0) {
    if (*str == ch)
      last_ch = str;
    ++str;
  }
  return (char *)last_ch;
}

/**
 * strchrs - Counts the occurrences of the character ch in the string pointed
 * to by src
 * @src: a pointer to the null-terminated byte string to be scanned
 * @ch: the character to be counted
 *
 * Return: the number of times the character ch occurs in the string pointed
 * to by src.
 */
uint32_t strchrs(const char *src, uint8_t ch) {
  ASSERT(src != NULL);
  int32_t ch_cnt = 0;
  const char *p = src;
  while (*p != 0) {
    if (*p == ch)
      ch_cnt++;
    p++;
  }
  return ch_cnt;
}
