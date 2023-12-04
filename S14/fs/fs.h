/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"

/* total number of inodes */
#define MAX_FILES_PER_PART 4096

#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE

enum file_type { FT_UNKNOWN, FT_REGULAR, FT_DIRECTORY };

void filesys_init();

#endif
