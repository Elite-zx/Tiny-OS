/*
 * Author: Xun Morris
 * Time: 2023-12-05
 */
#include "file.h"
#include "bitmap.h"
#include "dir.h"
#include "fs.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

struct file file_table[MAX_FILES_OPEN];
extern struct partition *cur_part;

/**
 * get_free_slot_in_global_FT() - Get a free file slot in the global
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
  /* 0~2 --- stdint, stdout, stderr  */
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
 * setting the corresponding bit to 1 in the bitmap. Returns the index of the
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

/**
 * file_create() - Create a file in the given directory with specified name and
 * flag.
 * @parent_dir: Pointer to the parent directory where the file is to be created.
 * @filename: Name of the file to be created.
 * @flag: Flags that specify the attributes of the file.
 *
 * Attempts to create a new file with the given name and flag in the specified
 * directory. The function allocates an inode for the new file, adds a directory
 * entry for it in the parent directory, and updates the necessary metadata to
 * the disk. It also adds the newly created file to the file table and open
 * inodes list. The function returns a file descriptor on success or -1 on
 * failure. If a failure occurs during the process, it performs a rollback to
 * undo the changes made up to the point of failure.
 *
 * Rollback steps:
 * 3. Clear the file table entry if directory entry synchronization fails.
 * 2. Free the newly allocated inode.
 * 1. Reset the allocated inode in the inode bitmap.
 */
int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag) {
  /* Prepare buffer (two sectors size) for writing data to disk*/
  void *io_buf = sys_malloc(1024);
  if (io_buf == NULL) {
    printk("file_create: sys_malloc for io_buf failed\n");
    return -1;
  }

  /* This variable Determine whether rollback is required and what measures to
   * take to roll back  */
  uint8_t rollback_action = 0;

  /****************** create new inode ******************/
  int32_t new_inode_NO = inode_bitmap_alloc(cur_part);
  if (new_inode_NO == -1) {
    printk("file_create: allocate inode bit failed\n");
    return -1;
  }
  struct inode *new_inode = (struct inode *)sys_malloc(sizeof(struct inode));
  if (new_inode == NULL) {
    printk("file_create: sys_malloc for inode failed\n");
    rollback_action = 3;
    /*  rollback ---free new inode bit in inode_bitmap  */
    goto rollback;
  }
  inode_init(new_inode_NO, new_inode);

  /****************** create file table entry ******************/
  int fd_idx = get_free_slot_in_global_FT();
  if (fd_idx == -1) {
    printk("exceed max open files\n");
    rollback_action = 2;
    /* rollback ---free new inode space  */
    goto rollback;
  }
  file_table[fd_idx].fd_flag = flag;
  file_table[fd_idx].fd_inode = new_inode;
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_inode->write_deny = false;

  /****************** create directory entry ******************/
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));
  create_dir_entry(filename, new_inode_NO, FT_REGULAR, &new_dir_entry);

  /****************** Synchronize memory data to disk ******************/
  /* 1.Write a directory entry to a parent directory */
  if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
    printk("sync dir_entry to disk failed\n");
    /* rollback ---free file table entry */
    rollback_action = 1;
    goto rollback;
  }
  memset(io_buf, 0, 1024);
  /* 2. Write the new inode and parent dir's inode to partition cur_part */
  inode_sync(cur_part, parent_dir->_inode, io_buf);
  memset(io_buf, 0, 1024);
  inode_sync(cur_part, new_inode, io_buf);
  /* 3. Synchronize a new inode bit of the inode bitmap to disk.  */
  bitmap_sync(cur_part, new_inode_NO, INODE_BITMAP);

  list_push(&cur_part->open_inodes, &new_inode->inode_tag);
  new_inode->i_open_cnt = 1;

  sys_free(io_buf);

  /* Install a global file descriptor index to the current thread's local fd
   * table. */
  return pcb_fd_install(fd_idx);

/*************** perform rollback  (AKA exception handling) ****************/
rollback:
  switch (rollback_action) {
  case 1:
    memset(&file_table[fd_idx], 0, sizeof(struct file));
  case 2:
    sys_free(new_inode);
  case 3:
    bitmap_set(&cur_part->inode_bitmap, new_inode_NO, 1);
    break;
  }
  sys_free(io_buf);
  return -1;
}
