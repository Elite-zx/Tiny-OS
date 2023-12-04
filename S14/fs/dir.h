/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "fs.h"
#include "stdint.h"
#define MAX_FILE_NAME_LEN 16

/**
 * struct dir - Structure representing a directory.
 * @_inode: Pointer to the inode of the directory.
 * @dir_pos: Current offset within the directory.
 * @dir_buf: Data buffer for the directory's contents.
 *
 * This structure is used to manage and navigate through directories,
 * storing necessary information for directory operations.
 */
struct dir {
  struct inode *_inode;
  uint32_t dir_pos;
  uint32_t dir_buf[512];
};

/**
 * struct dir_entry - Structure for a directory entry.
 * @filename: Name of the file or directory.
 * @i_NO: Corresponding inode number of the file or directory.
 * @f_type: Type of the file (regular file or directory).
 *
 * Represents an entry in a directory, linking a name to an inode and
 * indicating the type of the file.
 */
struct dir_entry {
  char filename[MAX_FILE_NAME_LEN];
  uint32_t i_NO;
  enum file_type f_type;
};

#endif
