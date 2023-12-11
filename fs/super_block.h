/*
 * Author: Zhang Xun
 * Time: 2023-12-04
 */
#ifndef __FS_SUPER_BLOCK
#define __FS_SUPER_BLOCK

#include "stdint.h"

/**
 * struct super_block - Represents a filesystem's superblock.
 * @magic: Filesystem type identifier, used by OS to recognize filesystem types.
 * @sector_cnt: Total number of sectors in the partition.
 * @inode_cnt: Number of inodes in this partition.
 * @partition_LBA_addr: Starting LBA address of this partition.
 * @free_blocks_bitmap_LBA: Starting sector address of the block bitmap.
 * @free_blocks_bitmap_sectors: Number of sectors occupied by the block bitmap.
 * @inode_bitmap_LBA: Starting LBA address of the inode bitmap.
 * @inode_bitmap_sectors: Number of sectors occupied by the inode bitmap.
 * @inode_table_LBA: Starting LBA address of the inode table.
 * @inode_table_sectors: Number of sectors occupied by the inode table.
 * @data_start_LBA: Starting sector number of the data area.
 * @root_inode_NO: Inode number of the root directory.
 * @dir_entry_size: Size of directory entries.
 * @pad: Padding to make the structure size equal to 512 bytes (1 sector).
 *
 * The super_block structure is crucial in filesystem management, holding
 * vital information about the filesystem layout and metadata.
 */
struct super_block {
  uint32_t magic;

  uint32_t sector_cnt;
  uint32_t inode_cnt;
  uint32_t partition_LBA_addr;

  uint32_t free_blocks_bitmap_LBA;
  uint32_t free_blocks_bitmap_sectors;

  uint32_t inode_bitmap_LBA;
  uint32_t inode_bitmap_sectors;

  uint32_t inode_table_LBA;
  uint32_t inode_table_sectors;

  uint32_t data_start_LBA;
  uint32_t root_inode_NO;
  uint32_t dir_entry_size;

  uint8_t pad[460];
} __attribute__((packed));

#endif
