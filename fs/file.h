
#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "bitmap.h"
#include "dir.h"
#include "ide.h"
#include "interrupt.h"
#include "stdint.h"

/**
 * struct file - Structure representing an open file.
 * @fd_pos: Current offset position in the file, starting from 0 up to the file
 * size minus one.
 * @fd_flag: File operation flags, such as O_RDONLY for read-only access.
 * @fd_inode: Pointer to the inode in the partition's open inodes queue.
 *
 *() This structure is used to maintain information about an open file,
 including
 * the current position within the file (for reading or writing), flags that
 * indicate how the file was opened, and a pointer to the inode representing
 j the file on disk.
 */
struct file {
  uint32_t fd_pos;
  uint32_t fd_flag;
  struct inode *fd_inode;
};

enum std_fd { STDIN_NO, STDOUT_NO, STDERR_NO };

enum bitmap_type { INODE_BITMAP, BLOCK_BITMAP };

#define MAX_FILES_OPEN 32
int32_t inode_bitmap_alloc(struct partition *part);
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t get_free_slot_in_global_FT();
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp_flag);
int32_t block_bitmap_alloc(struct partition *part);

int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag);
int32_t file_open(uint32_t inode_NO, uint8_t flag);
int32_t file_close(struct file *file);
int32_t file_write(struct file *file, const void *buf, uint32_t count);
int32_t file_read(struct file *file, void *buf, uint32_t count);

#endif
