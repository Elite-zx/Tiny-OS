/*
 * Author: Xun Morris
 * Time: 2023-12-05
 */
#include "dir.h"
#include "bitmap.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"

struct dir root_dir;
extern struct partition *cur_part;

/**
 * open_root_dir() - Open the root directory of a partition.
 * @part: Pointer to the partition containing the root directory.
 *
 * This function opens the root directory of the specified partition.
 * It sets the root directory's inode to the inode associated with the
 * partition's root inode number and initializes the directory position.
 */
void open_root_dir(struct partition *part) {
  root_dir._inode = inode_open(part, part->sup_b->root_inode_NO);
  root_dir.dir_pos = 0;
}

/**
 * dir_open() - Open a directory in a partition.
 * @part: Pointer to the partition containing the directory.
 * @inode_no: The inode number of the directory to open.
 *
 * Opens a directory by its inode number in the given partition and returns
 * a pointer to the directory structure. Initializes the directory position.
 */
struct dir *dir_open(struct partition *part, uint32_t inode_NO) {
  struct dir *pdir = (struct dir *)sys_malloc(sizeof(struct dir));
  pdir->_inode = inode_open(part, inode_NO);
  /* dir_buf in pdir are initialized to 0 by sys_malloc  */
  pdir->dir_pos = 0;
  return pdir;
}

/**
 * search_dir_entry() - Search for a file or directory entry in a directory.
 * @part: Pointer to the partition in which to search.
 * @pdir: Pointer to the directory in which to search.
 * @name: The name of the file or directory to find.
 * @dir_e: Pointer to a dir_entry structure to store the found entry.
 *
 * Searches for a directory entry with the specified name in the given
 * directory. If found, the function returns true and copies the directory entry
 * to dir_e. Otherwise, it returns false. The function considers both direct and
 * indirect blocks of the directory's inode.
 */
bool search_dir_entry(struct partition *part, struct dir *pdir,
                      const char *name, struct dir_entry *dir_e) {

  /*********************************************************/
  /* stores the address of all blocks into all_inode_blocks*/
  /*********************************************************/

  /*12 direct blocks + 128 first-level indirect blocks */
  uint32_t inode_blocks_cnt = 12 + (512 / 4);

  /*  */
  uint32_t *all_inode_blocks = (uint32_t *)sys_malloc(inode_blocks_cnt * 4);
  if (all_inode_blocks == NULL) {
    printk("search_dir_entry: sys_malloc for all_inode_blocks failed");
    return false;
  }

  uint32_t block_idx = 0;
  /* direct blocks  */
  while (block_idx < 12) {
    all_inode_blocks[block_idx] = pdir->_inode->i_blocks[block_idx];
    block_idx++;
  }
  /* first_level indirect block exist, so read the first_level block-index-table
   * from disk */
  if (pdir->_inode->i_blocks[12] != 0) {
    ide_read(part->which_disk, pdir->_inode->i_blocks[12],
             all_inode_blocks + 12, 1);
  }

  /********************************************************  */
  /* find directory entry dir_e in directory pdir  */
  /********************************************************  */
  uint8_t *buf = (uint8_t *)sys_malloc(BLOCK_SIZE);
  struct dir_entry *de_iter = (struct dir_entry *)buf;
  uint32_t _dir_entry_size = part->sup_b->dir_entry_size;
  /* dir entry count in each block   */
  uint32_t dir_entry_cnt = BLOCK_SIZE / _dir_entry_size;
  block_idx = 0;

  /* iterate over each blocks  */
  while (block_idx < inode_blocks_cnt) {
    if (all_inode_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }

    /* read block from disk, dir entries is in block  */
    ide_read(part->which_disk, all_inode_blocks[block_idx], buf, 1);

    /* iterate over directory entries stored in a block  */
    uint32_t dir_entry_idx = 0;
    while (dir_entry_idx < dir_entry_cnt) {
      if (!strcmp(de_iter->filename, name)) {
        memcpy(dir_e, de_iter, _dir_entry_size);
        sys_free(buf);
        sys_free(all_inode_blocks);
        return true;
      }
      dir_entry_idx++;
      de_iter++;
    }
    block_idx++;
    de_iter = (struct dir_entry *)buf;
    memset(buf, 0, BLOCK_SIZE);
  }
  sys_free(buf);
  sys_free(all_inode_blocks);
  return false;
}

/**
 * dir_close() - Close a directory.
 * @dir: Pointer to the directory to close.
 *
 * Closes the specified directory. If the directory is the root directory,
 * it doesn't perform any operation, as the root directory should not be
 * closed. Otherwise, it closes the directory's inode and frees the directory
 * structure.
 */
void dir_close(struct dir *dir) {
  /* root directory '/' can not be closed, so do nothing to it  */
  if (dir == &root_dir)
    return;
  inode_close(dir->_inode);
  sys_free(dir);
}

/**
 * create_dir_entry() - Initialize a directory entry.
 * @filename: Name of the file or directory for the entry.
 * @inode_no: Inode number of the file or directory.
 * @file_type: Type of the file (e.g., file or directory).
 * @p_de: Pointer to the dir_entry structure to initialize.
 *
 * Initializes a directory entry with the specified filename, inode number,
 * and file type. This function prepares the dir_entry structure for use
 * in directory operations.
 */
void create_dir_entry(char *filename, uint32_t inode_NO, uint8_t file_type,
                      struct dir_entry *p_de) {
  ASSERT(strlen(filename) < MAX_FILE_NAME_LEN);
  memcpy(p_de->filename, filename, strlen(filename));
  p_de->i_NO = inode_NO;
  p_de->f_type = file_type;
}

/**
 * sync_dir_entry() - Write a directory entry to a parent directory.
 * @parent_dir: Pointer to the parent directory where the entry is to be
 * written.
 * @p_de: Pointer to the directory entry to be written.
 * @io_buf: Buffer provided by the caller for I/O operations.
 *
 * Writes the directory entry 'p_de' to the specified parent directory
 * 'parent_dir'. The function looks for an empty slot in the directory to place
 * the new entry. If necessary, it allocates a new block to store the entry if
 * the existing blocks are full. The function handles both direct and indirect
 * blocks of the directory. It returns true if the directory entry is
 * successfully written, otherwise false.
 */
bool sync_dir_entry(struct dir *parent_dir, struct dir_entry *de,
                    void *io_buf) {
  struct inode *dir_inode = parent_dir->_inode;
  uint32_t dir_size = dir_inode->i_size;
  uint32_t _dir_entry_size = cur_part->sup_b->dir_entry_size;
  ASSERT(dir_size % _dir_entry_size == 0);

  uint32_t max_dir_entries_per_sector = SECTOR_SIZE / _dir_entry_size;
  int32_t block_LBA = -1;

  uint8_t block_idx = 0;
  /* 12 direct blocks + 128 first_level indirect blocks, all blocks's address of
   * this file (parent directory) are stored in all_blocks*/
  uint32_t all_blocks[140] = {0};
  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_blocks[block_idx];
    block_idx++;
  }

  int32_t block_bitmap_idx = -1;

  /**** find free slot in parent directory to place the directory entry de ****/
  block_idx = 0;
  while (block_idx < 140) {
    block_bitmap_idx = -1;
    if (all_blocks[block_idx] == 0) {
      /* the corresponding block of block_idx does not exists, allocate block
       * for this file  */
      block_LBA = block_bitmap_alloc(cur_part);
      if (block_LBA == -1) {
        printk("allocate block bitmap for sync_dir_entry failed\n");
        return false;
      }

      block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
      ASSERT(block_bitmap_idx != -1);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      /**** update i_blocks info (block pointer) for inode  ****/
      block_bitmap_idx = -1;
      if (block_idx < 12) {
        /* the allocated block is a direct block  */
        dir_inode->i_blocks[block_idx] = all_blocks[block_idx] = block_LBA;
      } else if (block_idx == 12) {
        /* the allocated block is the first_level block-index-table, which point
         * to the first_level indirect block  */
        dir_inode->i_blocks[12] = block_LBA;
        block_LBA = -1;
        block_LBA = block_bitmap_alloc(cur_part);
        if (block_LBA == -1) {
          /* allocate first_level indirect block failed, roll back  */
          block_bitmap_idx =
              dir_inode->i_blocks[12] - cur_part->sup_b->data_start_LBA;
          bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
          dir_inode->i_blocks[12] = 0;
          printk("allocate block bitmap for sync_dir_entry failed");
          return false;
        }
        block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
        ASSERT(block_bitmap_idx != -1);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        all_blocks[12] = block_LBA;
        ide_write(cur_part->which_disk, dir_inode->i_blocks[12],
                  all_blocks + 12, 1);
      } else {
        /* the allocated block is the first_level indirect block*/
        all_blocks[block_idx] = block_LBA;
        ide_write(cur_part->which_disk, dir_inode->i_blocks[12],
                  all_blocks + 12, 1);
      }
      /* write new directory entry to the newly allocated block  */
      memset(io_buf, 0, 512);
      memcpy(io_buf, de, _dir_entry_size);
      ide_write(cur_part->which_disk, block_LBA, io_buf, 1);

      dir_inode->i_size += _dir_entry_size;
      return true;
    } else {
      /* the corresponding block of block_idx exists */
      ide_read(cur_part->which_disk, all_blocks[block_idx], io_buf, 1);
      struct dir_entry *dir_entry_base = (struct dir_entry *)io_buf;
      /* find free slot (empty directory entry) in this block  */
      uint8_t dir_entry_idx = 0;
      while (dir_entry_idx < max_dir_entries_per_sector) {
        if ((dir_entry_base + dir_entry_idx)->f_type == FT_UNKNOWN) {
          /* empty directory entry is here ^_^ */
          memcpy(dir_entry_base + dir_entry_idx, de, _dir_entry_size);
          ide_write(cur_part->which_disk, all_blocks[block_idx], io_buf, 1);
          dir_inode->i_size += _dir_entry_size;
          return true;
        }
        /* next directory entry  */
        dir_entry_idx++;
      }
      /* This data block is filled with directory entries, traverse the next
       * data block  */
      block_idx++;
    }
  }
  /** no free slot found in this parent directory file*/
  printk("directory is full!\n");
  return false;
}

bool delete_dir_entry(struct partition *part, struct dir *pdir,
                      uint32_t inode_NO, void *io_buf) {
  struct inode *dir_inode = pdir->_inode;

  /******** fill all_blocks_addr with the addresses of blocks ********/
  uint32_t block_idx = 0;
  uint32_t all_blocks_addr[140] = {0};
  while (block_idx < 12) {
    all_blocks_addr[block_idx] = dir_inode->i_blocks[block_idx];
    block_idx++;
  }
  if (dir_inode->i_blocks[12] != 0) {
    ide_read(part->which_disk, dir_inode->i_blocks[12], all_blocks_addr + 12,
             1);
  }

  /******** traverse blocks, find target dir entry ********/
  uint32_t _dir_entry_size = part->sup_b->dir_entry_size;
  uint32_t max_dir_entries_per_sector = SECTOR_SIZE / _dir_entry_size;
  struct dir_entry *dir_entry_base = (struct dir_entry *)io_buf;
  struct dir_entry *dir_entry_found = NULL;
  uint8_t dir_entry_idx, dir_entry_cnt;
  bool is_dir_first_block = false;

  block_idx = 0;
  while (block_idx < 140) {
    is_dir_first_block = false;
    if (all_blocks_addr[block_idx] == 0) {
      block_idx++;
      continue;
    }
    dir_entry_idx = dir_entry_cnt = 0;
    memset(io_buf, 0, BLOCK_SIZE);
    ide_read(part->which_disk, all_blocks_addr[block_idx], io_buf, 1);

    /**** traverse each directory entry in the sector (which is alse a
     * block)****/
    while (dir_entry_idx < max_dir_entries_per_sector) {
      if ((dir_entry_base + dir_entry_idx)->f_type != FT_UNKNOWN) {
        if (!strcmp((dir_entry_base + dir_entry_idx)->filename, ".")) {
          /* current block is the first block of pdir*/
          is_dir_first_block = true;
        } else if (strcmp((dir_entry_base + dir_entry_idx)->filename, ".") &&
                   strcmp((dir_entry_base + dir_entry_idx)->filename, "..")) {
          /* count the number of entries on the current block  */
          dir_entry_cnt++;
          if ((dir_entry_base + dir_entry_idx)->i_NO == inode_NO) {
            /* find target entry by comparing inode number  */
            ASSERT(dir_entry_found == NULL);
            dir_entry_found = dir_entry_base + dir_entry_idx;
          }
        }
      }
      dir_entry_idx++;
    }
    if (dir_entry_found == NULL) {
      /* target dir entry is not found in this block, go to the next block  */
      block_idx++;
      continue;
    }
    /******** target dir entry found ********/
    ASSERT(dir_entry_cnt >= 1);
    if (dir_entry_cnt == 1 && !is_dir_first_block) {
      /**** this block is not the first block of the directory, and there is
       * only target dir entry on this sector, so this block needs to be
       * reclaimed ****/
      uint32_t block_bitmap_idx =
          all_blocks_addr[block_idx] - part->sup_b->data_start_LBA;
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);
      if (block_idx < 12) {
        dir_inode->i_blocks[block_idx] = 0;
      } else {
        /* count the number of indirect blocks  */
        uint32_t indirect_blocks_cnt = 0;
        uint32_t indirect_block_idx = 12;
        while (indirect_block_idx < 140) {
          if (all_blocks_addr[indirect_block_idx] != 0) {
            indirect_blocks_cnt++;
          }
        }
        ASSERT(indirect_blocks_cnt >= 1);
        if (indirect_blocks_cnt > 1) {
          /* erase the current indirect block address in the first-level
           * indirect block index table only  */
          all_blocks_addr[block_idx] = 0;
          ide_write(part->which_disk, dir_inode->i_blocks[12],
                    all_blocks_addr + 12, 1);
        } else {
          /*erase the first_level indirect block index table */
          block_bitmap_idx =
              dir_inode->i_blocks[12] - part->sup_b->data_start_LBA;
          bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
          bitmap_sync(part, block_bitmap_idx, BLOCK_BITMAP);
          dir_inode->i_blocks[12] = 0;
        }
      }
    } else {
      /**** current block exist Multiple directory entries ****/
      memset(dir_entry_found, 0, _dir_entry_size);
      ide_write(part->which_disk, all_blocks_addr[block_idx], io_buf, 1);
    }
    ASSERT(dir_inode->i_size >= _dir_entry_size);
    dir_inode->i_size -= _dir_entry_size;
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(part, dir_inode, io_buf);
    return true;
  }
  /* target dir entry is not found  */
  return false;
}
