/*
 * Author: Xun Morris |
 * Time: 2023-11-13 |
 */

#ifndef _LIB_KERNEL_IO_H
#define _LIB_KERNEL_IO_H

#include "stdint.h"

/*
 * outb - Write a byte of data to the port
 * @port: 16bits, corresponding to the maximum port numbers (65536)
 * supported by intel.
 * @data: data to write
 */
static inline void outb(uint16_t port, uint8_t data) {
  asm volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

/*
 * outsw - Read word_cnt words from  memory and write the data to the port
 * @port: port to be written
 * @addr: the beginning address of memory
 * @word_cnt: the number of words to read and write
 *
 * outsw means OUTput String Word in memory (pointed by ds:esi) to the port.
 */
static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt) {
  asm volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
}

/* inb - Read a byte of data from the port and write the data to the memory
 * @port: port to read
 *
 * Return: data read from port.
 */
static inline uint8_t inb(uint16_t port) {
  uint8_t data;
  asm volatile("inb %w1,%b0" : "=a"(data) : "Nd"(port));
  return data;
}

/*
 * insw - Read word_cnt words from the port and write the data memory
 * @port: port to read
 * @addr: the beginning address of memory
 * @word_cnt: the number of words to read and write
 *
 * insw means input String Word (stored in port) to the memory (pointed by
 * es:edi).
 */
static inline void insw(uint16_t port, void *addr, uint32_t word_cnt) {
  asm volatile("cld; rep insw"
               : "+D"(addr), "+c"(word_cnt)
               : "d"(port)
               : "memory");
}

#endif
