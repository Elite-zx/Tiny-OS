/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "global.h"
#include "list.h"
#include "stdint.h"

/**
 * struct inode - Inode structure for a filesystem object.
 * @i_NO: Inode number.
 * @i_size: Size of the file. For directories, the total size of all directory
 * entries.
 * @i_open_cnts: Count of how many times this file has been opened.
 * @write_deny: Flag to indicate if writing to the file is currently denied.
 * @i_block: Direct and indirect block pointers (i_sectors[0-11] are direct).
 * @inode_tag: List element for including this inode in a list.
 *
 * The inode structure stores metadata about a filesystem object, such as a file
 * or directory. It includes information about size, open counts, and block
 * allocation.
 */
struct inode {
  uint32_t i_NO;
  uint32_t i_size;
  uint32_t i_open_cnt;
  bool write_deny;

  uint32_t i_block[13];
  struct list_elem inode_tag;
};

#endif
