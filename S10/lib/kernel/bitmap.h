#ifndef __LIb_KERNEL_BITMAP_H
#define __LIb_KERNEL_BITMAP_H

#include "global.h"
#include "stdint.h"

#define BITMAP_MASK 1

/**
 * struct bitmap - Represents a bitmap data structure.
 * @bmap_bytes_len: Length of the bitmap in bytes.
 * @bits: Pointer to the array of bytes representing the bitmap.
 *
 * This structure is used to manage a bitmap, a collection of bits that
 * represents binary data efficiently.
 */
struct bitmap {
  uint32_t bmap_bytes_len;
  uint8_t *bits;
};

void bitmap_init(struct bitmap *btmp);
bool bitmap_bit_test(struct bitmap *btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap *btmp, uint32_t cnt);
void bitmap_set(struct bitmap *btmp, uint32_t bit_idx, int8_t value);

#endif
