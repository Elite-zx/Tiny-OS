/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "fs.h"
#include "global.h"
#include "ide.h"
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
  enum file_types f_type;
};

bool search_dir_entry(struct partition *part, struct dir *pdir,
                      const char *name, struct dir_entry *dir_e);
void dir_close(struct dir *dir);
struct dir *dir_open(struct partition *part, uint32_t inode_NO);
void create_dir_entry(char *filename, uint32_t inode_NO, uint8_t file_type,
                      struct dir_entry *p_de);
bool sync_dir_entry(struct dir *parent_dir, struct dir_entry *de, void *io_buf);

void open_root_dir(struct partition *part);
bool delete_dir_entry(struct partition *part, struct dir *pdir,
                      uint32_t inode_NO, void *io_buf);
#endif
