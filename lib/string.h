/*
 * Author: Zhang Xun
 * Time: 2023-11-16
 */

#ifndef __LIB_STRING_H
#define __LIB_STRING_H
#include "stdint.h"
void memset(void *dst, uint8_t value, uint32_t size);
void memcpy(void *dst, const void *src, uint32_t size);
int memcmp(const void *a, const void *b, unsigned long size);

char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
int8_t strcmp(const char *a, const char *b);
uint32_t strlen(const char *str);
char *strchr(const char *str, const uint8_t ch);
char *strrchr(const char *str, int ch);
uint32_t strchrs(const char *src, uint8_t ch);
#endif
