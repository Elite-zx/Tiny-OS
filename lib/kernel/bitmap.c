#include "bitmap.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "string.h"

/**
 * bitmap_init - Initializes a bitmap.
 * @btmp: A pointer to the bitmap to be initialized.
 *
 * Sets all bits in the provided bitmap to 0.
 */
void bitmap_init(struct bitmap *btmp) {
  memset(btmp->bits, 0, btmp->bmap_bytes_len);
}

/**
 * bitmap_bit_test - Tests a bit at a specific index in a bitmap.
 * @btmp: A pointer to the bitmap.
 * @bit_idx: The index of the bit to test.
 *
 * Return: True if the bit is set (1), false otherwise.
 */
bool bitmap_bit_test(struct bitmap *btmp, uint32_t bit_idx) {
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_idx_in_byte = bit_idx % 8;

  return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_idx_in_byte));
}

/**
 * bitmap_scan - Scans the bitmap for a consecutive number of unset bits.
 * @btmp: A pointer to the bitmap.
 * @cnt: The number of consecutive unset (0) bits to find.
 *
 * Scans for a sequence of unset bits that are at least 'cnt' bits long.
 *
 * Return: The starting index of the sequence if found, -1 otherwise.
 */
int bitmap_scan(struct bitmap *btmp, uint32_t cnt) {
  uint32_t byte_idx = 0;
  /* testing by bytes to exclude bytes that are all 1  */
  while ((0xff == btmp->bits[byte_idx]) && (byte_idx < btmp->bmap_bytes_len))
    ++byte_idx;

  ASSERT(byte_idx < btmp->bmap_bytes_len);
  /* all bytes are 1 so that there is no bit that is 0, return -1  */
  if (byte_idx == btmp->bmap_bytes_len)
    return -1;

  /* do bit by bit comparison to find bit 0 in the first byte where bit 0 is present.*/
  int bit_idx = 0;
  while ((uint8_t)(BITMAP_MASK << bit_idx) & btmp->bits[byte_idx])
    ++bit_idx;

  /* free_bit_idx_start is the index of the first bit 0 of the entire bitmap */
  int free_bit_idx_start = byte_idx * 8 + bit_idx;
  if (cnt == 1)
    return free_bit_idx_start;

  uint32_t bit_remaining = btmp->bmap_bytes_len * 8 - free_bit_idx_start;
  uint32_t next_bit = free_bit_idx_start + 1;
  uint32_t count = 1;

  /* Traverse the bits in the bitmap starting from the bit after
   * free_bit_idx_start and find the sequence  */
  free_bit_idx_start = -1;
  while (bit_remaining-- > 0) {
    if (!bitmap_bit_test(btmp, next_bit)) {
      ++count;
    } else {
      count = 0;
    }
    if (count == cnt) {
      free_bit_idx_start = next_bit - cnt + 1;
      break;
    }
    ++next_bit;
  }
  return free_bit_idx_start;
}

/**
 * bitmap_set - Sets or clears a specific bit in a bitmap.
 * @btmp: A pointer to the bitmap.
 * @bit_idx: The index of the bit to set or clear.
 * @value: The value to set the bit to (0 or 1).
 *
 * Sets the bit at index 'bit_idx' in the bitmap to 'value'.
 */
void bitmap_set(struct bitmap *btmp, uint32_t bit_idx, int8_t value) {
  ASSERT((value == 0) || (value == 1));
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_idx_in_byte = bit_idx % 8;

  if (value) {
    btmp->bits[byte_idx] |= BITMAP_MASK << bit_idx_in_byte;
  } else {
    btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_idx_in_byte);
  }
}
