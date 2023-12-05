#include "file.h"
#include "bitmap.h"
#include "fs.h"
#include "ide.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "super_block.h"
#include "thread.h"

struct file file_table[MAX_FILES_OPEN];

/**
 * get_free_slot_in_global_FT() - Get a free file descriptor slot in the global
 * file table.
 *
 * Iterates through the global file table to find an unused file descriptor
 * slot. Returns the index of the free slot if available, or -1 if the file
 * table is full and no slots are available. The search starts from index 3, as
 * the first three descriptors are reserved for standard input/output/error.
 */
int32_t get_free_slot_in_global_FT() {
  uint32_t fd_idx = 3;
  while (fd_idx < MAX_FILES_OPEN) {
    if (file_table[fd_idx].fd_inode == NULL)
      break;
    fd_idx++;
  }
  if (fd_idx == MAX_FILES_OPEN) {
    printk("exceed max open files\n");
    return -1;
  }
  return fd_idx;
}

/**
 * pcb_fd_install() - Install a global file descriptor index to the current
 * thread's local fd table.
 * @globa_fd_idx: The global file descriptor index to install.
 *
 * Installs a file descriptor, represented by a global index, into the calling
 * thread's local file descriptor table. Returns the local file descriptor index
 * on success, or -1 if all file descriptor slots in the process are in use.
 */
int32_t pcb_fd_install(int32_t global_fd_idx) {
  struct task_struct *cur = running_thread();
  uint8_t local_fd_idx = 3;
  while (local_fd_idx < MAX_FILES_OPEN) {
    if (cur->fd_table[local_fd_idx] == -1) {
      cur->fd_table[local_fd_idx] = global_fd_idx;
      break;
    }
    local_fd_idx++;
  }
  if (local_fd_idx == MAX_FILES_OPEN_PER_PROC) {
    printk("exceed max open files for each process\n");
    return -1;
  }
  return local_fd_idx;
}

/**
 * inode_bitmap_alloc() - Allocate an inode in the partition's inode bitmap.
 * @part: Pointer to the partition whose inode bitmap is to be modified.
 *
 * Scans the partition's inode bitmap for a free inode and allocates it by
 * setting the corresponding bit in the bitmap. Returns the index of the
 * allocated inode on success, or -1 if there are no free inodes available.
 */
int32_t inode_bitmap_alloc(struct partition *part) {
  int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
  if (bit_idx == -1)
    return -1;
  bitmap_set(&part->inode_bitmap, bit_idx, 1);
  return bit_idx;
}

/**
 * block_bitmap_alloc() - Allocate a block in the partition's block bitmap.
 * @part: Pointer to the partition whose block bitmap is to be modified.
 *
 * Scans the partition's block bitmap for a free block and allocates it by
 * setting the corresponding bit in the bitmap. Returns the sector address of
 * the allocated block on success, or -1 if there are no free blocks available.
 */
int32_t block_bitmap_alloc(struct partition *part) {
  int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
  if (bit_idx == -1) {
    return -1;
  }
  bitmap_set(&part->block_bitmap, bit_idx, 1);
  return (part->sup_b->data_start_LBA + bit_idx);
}

/**
 * bitmap_sync() - Synchronize a specific bit of the inode or block bitmap to
 * disk.
 * @part: Pointer to the partition containing the bitmap.
 * @bit_idx: The index of the bit in the bitmap to be synchronized.
 * @btmp_type: The type of bitmap (inode or block) to be synchronized.
 *
 * Synchronizes the specific bit (given by bit_idx) of the specified bitmap
 * (inode or block) to the disk. Since disk reading and writing are based on
 * sectors, this function calculates the sector address of the bit to be
 * synchronized and writes the corresponding 512-byte block of the bitmap to
 * disk.
 */
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp_flag) {
  uint32_t bit_offset_in_sector = bit_idx / (512 * 8);
  uint32_t bit_offset_in_byte = bit_offset_in_sector * BLOCK_SIZE;

  uint32_t sector_LBA;
  uint8_t *bitmap_offset;

  switch (btmp_flag) {
  case INODE_BITMAP:
    sector_LBA = part->sup_b->inode_bitmap_LBA + bit_offset_in_sector;
    bitmap_offset = part->inode_bitmap.bits + bit_offset_in_byte;
    break;
  case BLOCK_BITMAP:
    sector_LBA = part->sup_b->free_blocks_bitmap_LBA + bit_offset_in_sector;
    bitmap_offset = part->inode_bitmap.bits + bit_offset_in_byte;
    break;
  }
  ide_write(part->which_disk, sector_LBA, bitmap_offset, 1);
}
