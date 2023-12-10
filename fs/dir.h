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
 * struct dir - Represents a directory in the filesystem.
 * @inode: Pointer to the inode structure associated with the directory.
 * @dir_pos: Keeps track of the offset within the directory, indicating the
 *           position at which to read the next directory entry.
 * @dir_buf: Buffer to hold the contents of the directory's data. It acts as
 *           a cache to store a directory's content, typically one disk sector
 *           size, for quicker access to directory entries. (This member used
 *           by function dir_read)
 *
 * This structure is used to represent a directory in the filesystem. It is
 * essential for operations like opening a directory, reading directory entries,
 * and iterating over the contents of a directory. The 'inode' field points
 * to the directory's inode which contains metadata like size, permissions, etc.
 * The 'dir_pos' field is used to track the current position within the
 * directory for sequential access. The 'dir_buf' is a cache that stores the
 * actual data of the directory entries, improving the efficiency of directory
 * operations.
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
struct dir_entry *dir_read(struct dir *dir);
bool dir_is_empty(struct dir *dir);
int32_t dir_remove(struct dir *parent_dir, struct dir *child_dir);
#endif
