/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"

/* total number of inodes */
#define MAX_FILES_PER_PART 4096
#define MAX_PATH_LEN 512

#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE

enum file_types { FT_UNKNOWN, FT_REGULAR, FT_DIRECTORY };

enum oflags {
  O_RONLY = 0b000,
  O_WRONLY = 0b001,
  O_RDWR = 0b010,
  O_CREAT = 0b0100
};

/**
 * struct path_search_record - Record of the search path during file searching.
 * @searched_path: Buffer to store the parent path encountered during the search
 * process.
 * @parent_dir: Pointer to the direct parent directory of the file or directory
 * being searched.
 * @file_type: Type of the file found. This could be a regular file, a
 * directory, or unknown (FT_UNKNOWN) if the file is not found.
 *
 * This structure is used to keep track of the path traversed during the process
 * of searching for a file or directory. It records the path navigated
 * ('searched_path'), the direct parent directory of the file or directory in
 * question ('parent_dir'), and the type of the file if found ('file_type').
 */
struct path_search_record {
  char searched_path[MAX_PATH_LEN];
  struct dir *parent_dir;
  enum file_types file_type;
};

void filesys_init();
int32_t sys_open(const char *pathname, uint8_t flag);

#endif
