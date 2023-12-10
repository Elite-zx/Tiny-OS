/*
 * Author: Xun Morris
 * Time: 2023-12-05
 */
#include "file.h"
#include "bitmap.h"
#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
#include "syscall_init.h"
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
    bitmap_offset = part->block_bitmap.bits + bit_offset_in_byte;
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

/**
 * file_open() - Open a file by its inode number.
 * @inode_no: The inode number of the file to be opened.
 * @flag: Flag indicating the mode in which the file is to be opened.
 *
 * Opens a file based on its inode number and the provided flag.
 * It assigns a file descriptor from the global file table and sets
 * the file's position to the start. Handles file opening in different
 * modes such as read, write, or read/write. Manages the write_deny
 * flag to prevent simultaneous writes to the file.
 *
 * Return: The file descriptor on success, or -1 on failure.
 */
int32_t file_open(uint32_t inode_NO, uint8_t flag) {
  /******** initialize file table entry  ********/
  int fd_idx = get_free_slot_in_global_FT();
  if (fd_idx == -1) {
    printk("exceed max open files\n");
    return -1;
  }
  file_table[fd_idx].fd_flag = flag;
  file_table[fd_idx].fd_inode = inode_open(cur_part, inode_NO);
  file_table[fd_idx].fd_pos = 0;
  bool *write_deny = &file_table[fd_idx].fd_inode->write_deny;

  /******** Check if there are other processes writing to the file ********/
  if (flag & O_WRONLY || flag & O_RDWR) {
    enum intr_status old_status = intr_disable();
    if (!*write_deny) {
      *write_deny = true;
      intr_set_status(old_status);
    } else {
      intr_set_status(old_status);
      printk("file can't be write now, try again later\n");
      return -1;
    }
  }
  return pcb_fd_install(fd_idx);
}

/**
 * file_close() - Close an open file.
 * @file: Pointer to the file structure to be closed.
 *
 * Closes the file pointed to by 'file'. It resets the write_deny flag
 * and releases the file's inode. The file structure is made available
 * for future file operations.
 *
 * Return: 0 on successful closure, -1 if the file pointer is NULL.
 */
int32_t file_close(struct file *file) {
  if (file == NULL)
    return -1;
  file->fd_inode->write_deny = false;
  inode_close(file->fd_inode);
  file->fd_inode = NULL;
  return 0;
}

/**
 * file_write() - Write data to a file.
 * @file: Pointer to the file structure where data is to be written.
 * @buf: Buffer containing the data to be written.
 * @count: Number of bytes to write to the file.
 *
 * Writes 'count' bytes of data from 'buf' to the file pointed by 'file'.
 * Handles allocation of new blocks if the file size exceeds the current
 * allocation. Manages direct and indirect blocks, updates inode and file
 * size, and writes data to disk. It ensures that file size does not exceed
 * the maximum file size limit.
 *
 * Return: The number of bytes written on success, or -1 on failure.
 */
int32_t file_write(struct file *file, const void *buf, uint32_t count) {
  if (file->fd_inode->i_size > (BLOCK_SIZE * 140)) {
    printk("exceed max 71680, write file failed\n");
    return -1;
  }

  /* Memory operations are in blocks  */
  uint8_t *io_buf = (uint8_t *)sys_malloc(BLOCK_SIZE);
  if (io_buf == NULL) {
    printk("file_write: sys_malloc for io_buf failed\n");
    return -1;
  }

  /* all_blocks_addr is used to store the addresses of 128 indirect blocks
   * (first-level) and 12 direct blocks. Note that the first-level indirect
   * block index table (also a block) accommodates 128 indirect blocks, because
   * a block is 512 bytes and an address occupies 4 bytes, so 512/4=128  */
  uint32_t *all_blocks_addr = (uint32_t *)sys_malloc(BLOCK_SIZE + 12 * 4);
  if (io_buf == NULL) {
    printk("file_write: sys_malloc for all_blocks_addr failed\n");
    return -1;
  }

  uint32_t block_LBA = -1;
  uint32_t block_bitmap_idx = 0;
  int32_t indirect_block_table;
  uint32_t block_idx;

  /******** allocate block in case of first write,  ********/
  if (file->fd_inode->i_blocks[0] == 0) {
    block_LBA = block_bitmap_alloc(cur_part);
    if (block_LBA == -1) {
      printk("file_write: block_bitmap_alloc failed\n");
      return -1;
    }
    /* synchronize block bitmap  */
    file->fd_inode->i_blocks[0] = block_LBA;
    block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }

  /******** calculate the number of additional sectors required for written data
   * ********/
  uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
  uint32_t file_will_use_blocks =
      (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
  ASSERT(file_will_use_blocks < 140);
  uint32_t extra_blocks_required = file_will_use_blocks - file_has_used_blocks;

  /******** Store the address of the block to be used in all_block ********/
  if (extra_blocks_required == 0) {
    /******** no new blocks required ********/
    if (file_has_used_blocks < 12) {
      /**** use direct block --- Record the address of the direct block in
       * all_blocks_addr ****/
      block_idx = file_has_used_blocks - 1;
      all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];
    } else {
      /**** use indirect block --- Record the address of the indirect block in
       * all_blocks_addr ****/
      ASSERT(file->fd_inode->i_blocks[12] != 0);
      indirect_block_table = file->fd_inode->i_blocks[12];
      /* read first-level indirect block index table to all_blocks_addr */
      ide_read(cur_part->which_disk, indirect_block_table, all_blocks_addr + 12,
               1);
    }
  } else {
    /******** extra new blocks required ********/
    if (file_will_use_blocks <= 12) {
      /****  require new direct blocks ****/
      block_idx = file_has_used_blocks - 1;
      ASSERT(file->fd_inode->i_blocks[block_idx] != 0);
      all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];

      block_idx = file_has_used_blocks;
      while (block_idx < file_will_use_blocks) {
        block_LBA = block_bitmap_alloc(cur_part);
        if (block_LBA == -1) {
          printk("file_write: block_bitmap_alloc failed (situation 1)\n");
          return -1;
        }
        ASSERT(file->fd_inode->i_blocks[block_idx] == 0);
        file->fd_inode->i_blocks[block_idx] = all_blocks_addr[block_idx] =
            block_LBA;
        block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        block_idx++;
      }
    } else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
      /**** New data may use direct blocks (if any), but will definitely use
       * indirect blocks ****/
      block_idx = file_has_used_blocks - 1;
      all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];

      block_LBA = block_bitmap_alloc(cur_part);
      if (block_LBA == -1) {
        printk("file_write: block_bitmap_alloc failed (situation 2)\n");
        return -1;
      }
      ASSERT(file->fd_inode->i_blocks[12] == 0);
      indirect_block_table = file->fd_inode->i_blocks[12] = block_LBA;
      block_idx = file_has_used_blocks;

      while (block_idx < file_will_use_blocks) {
        block_LBA = block_bitmap_alloc(cur_part);
        if (block_LBA == -1) {
          printk("file_write: block_bitmap_alloc failed (situation 2)\n");
          return -1;
        }
        if (block_idx < 12) {
          /* allocate direct blocks  */
          ASSERT(file->fd_inode->i_blocks[block_idx] == 0);
          file->fd_inode->i_blocks[block_idx] = all_blocks_addr[block_idx] =
              block_LBA;
        } else {
          /* allocate indirect blocks  */
          all_blocks_addr[block_idx] = block_LBA;
        }
        block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        block_idx++;
      }
      /* synchronize first-level indirect block index table */
      ide_write(cur_part->which_disk, indirect_block_table,
                all_blocks_addr + 12, 1);
    } else if (file_has_used_blocks > 12) {
      /**** new data only use indirect blocks ****/
      ASSERT(file->fd_inode->i_blocks[12] != 0);
      indirect_block_table = file->fd_inode->i_blocks[12];
      ide_read(cur_part->which_disk, indirect_block_table, all_blocks_addr + 12,
               1);
      block_idx = file_has_used_blocks;
      while (block_idx < file_will_use_blocks) {
        block_LBA = block_bitmap_alloc(cur_part);
        if (block_LBA == -1) {
          printk("file_write: block_bitmap_alloc failed (situation 3)\n");
          return -1;
        }
        all_blocks_addr[block_idx] = block_LBA;
        block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        block_idx++;
      }
      /* synchronize first-level indirect block index table */
      ide_write(cur_part->which_disk, indirect_block_table,
                all_blocks_addr + 12, 1);
    }
  }

  /******** now! the addresses of all blocks are in all_blocks_addr, start
   * writing ********/
  const uint8_t *src = buf;
  uint32_t bytes_left_cnt = count;
  uint32_t sector_idx;
  uint32_t sector_LBA;
  uint32_t offset_bytes_in_sector;
  uint32_t left_bytes_in_sector;
  /* the size of data written  */
  uint32_t bytes_written_cnt = 0;
  /* data size written each time  */
  uint32_t chunk_size;

  /* the first written block is a special case: there may be old data in the
   * sector  */
  bool first_write_block = true;

  file->fd_pos = file->fd_inode->i_size - 1;
  while (bytes_written_cnt < count) {
    memset(io_buf, 0, BLOCK_SIZE);
    sector_idx = file->fd_inode->i_size / BLOCK_SIZE;
    sector_LBA = all_blocks_addr[sector_idx];
    offset_bytes_in_sector = file->fd_inode->i_size % BLOCK_SIZE;
    left_bytes_in_sector = BLOCK_SIZE - offset_bytes_in_sector;

    chunk_size = bytes_left_cnt < left_bytes_in_sector ? bytes_left_cnt
                                                       : left_bytes_in_sector;
    if (first_write_block) {
      /* Read sector, which contain old data for subsequent merging with new
       * data  */
      ide_read(cur_part->which_disk, sector_LBA, io_buf, 1);
      first_write_block = false;
    }
    /* Merge old data and new data in the same sector (only for the first write)
     */
    memcpy(io_buf + offset_bytes_in_sector, src, chunk_size);
    ide_write(cur_part->which_disk, sector_LBA, io_buf, 1);

    printk("file write at LBA 0x%x\n", sector_LBA);
    src += chunk_size;
    file->fd_inode->i_size += chunk_size;
    file->fd_pos += chunk_size;
    bytes_written_cnt += chunk_size;
    bytes_left_cnt -= chunk_size;
  }
  inode_sync(cur_part, file->fd_inode, io_buf);
  sys_free(all_blocks_addr);
  sys_free(io_buf);
  return bytes_written_cnt;
}

int32_t file_read(struct file *file, void *buf, uint32_t count) {
  uint32_t size = count;
  uint32_t size_left = size;

  /* When the number of bytes to be read exceeds the file size, the remaining
   * amount of the file is used as the number of bytes to be read.  */
  if ((file->fd_pos + count) > file->fd_inode->i_size) {
    size = file->fd_inode->i_size - file->fd_pos;
    size_left = size;
    if (size == 0)
      return -1;
  }

  uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
  if (io_buf == NULL) {
    printk("file_read: sys_malloc for io_buf failed\n");
  }

  uint32_t *all_blocks_addr = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
  if (all_blocks_addr == NULL) {
    printk("file_read: sys_malloc for io_buf failed\n");
    return -1;
  }

  uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
  uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
  uint32_t blocks_required_read = block_read_start_idx - block_read_end_idx;
  /* a file can have up to 140 blocks */
  ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);

  /******** Fill all_blocks_addr with block address ********/
  uint32_t block_idx;
  int32_t indirect_block_table;
  if (blocks_required_read == 0) {
    /**** The content to be read is in the same block****/
    ASSERT(block_read_start_idx == block_read_end_idx);
    if (block_read_start_idx < 12) {
      /* within 12 direct blocks  */
      block_idx = block_read_start_idx;
      all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];
    } else {
      /* indirect blocks pointed by indirect_block_table. read
       * indirect_block_table from disk to memory */
      indirect_block_table = file->fd_inode->i_blocks[12];
      ide_read(cur_part->which_disk, indirect_block_table, all_blocks_addr + 12,
               1);
    }
  } else {
    /**** The content to be read spans multiple blocks  ****/
    if (block_read_end_idx < 12) {
      /** 1. blocks to be read are all direct blocks **/
      block_idx = block_read_start_idx;
      while (block_idx < block_read_end_idx) {
        all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];
        block_idx++;
      }
    } else if (block_read_start_idx < 12 && block_read_end_idx > 12) {
      /** 2. blocks to be read spans direct and indirect blocks **/
      block_idx = block_read_start_idx;
      while (block_idx < 12) {
        /* get direct blocks and store it in all_blocks_addr */
        all_blocks_addr[block_idx] = file->fd_inode->i_blocks[block_idx];
        block_idx++;
      }
      /* get indirect blocks and store it in all_blocks_addr */
      ASSERT(file->fd_inode->i_blocks[12] != 0);
      indirect_block_table = file->fd_inode->i_blocks[12];
      ide_read(cur_part->which_disk, indirect_block_table, all_blocks_addr + 12,
               1);
    } else {
      /** 3. blocks to be read are all indirect blocks **/
      ASSERT(file->fd_inode->i_blocks[12] != 0);
      indirect_block_table = file->fd_inode->i_blocks[12];
      ide_read(cur_part->which_disk, indirect_block_table, all_blocks_addr + 12,
               1);
    }
  }
  /******** now! the addresses of all blocks are in all_blocks_addr, start
   * reading********/
  uint8_t *dst = buf;
  uint32_t sector_idx;
  uint32_t sector_LBA;
  uint32_t offset_bytes_in_sector;
  uint32_t left_bytes_in_sector;
  /* the size of data read  */
  uint32_t bytes_read_cnt = 0;
  /* data size read each time  */
  uint32_t chunk_size;

  while (bytes_read_cnt < size) {
    sector_idx = file->fd_pos / BLOCK_SIZE;
    sector_LBA = all_blocks_addr[sector_idx];
    offset_bytes_in_sector = file->fd_pos % BLOCK_SIZE;
    left_bytes_in_sector = BLOCK_SIZE - offset_bytes_in_sector;

    chunk_size =
        size_left < left_bytes_in_sector ? size_left : left_bytes_in_sector;

    memset(io_buf, 0, BLOCK_SIZE);
    ide_read(cur_part->which_disk, sector_LBA, io_buf, 1);
    memcpy(dst, io_buf + offset_bytes_in_sector, chunk_size);

    dst += chunk_size;
    file->fd_pos += chunk_size;
    bytes_read_cnt += chunk_size;
    size_left -= chunk_size;
  }
  sys_free(all_blocks_addr);
  sys_free(io_buf);
  return bytes_read_cnt;
}
