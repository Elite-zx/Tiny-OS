/*
 * Author: Zhang Xun
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
  O_RDONLY = 0b000,
  O_WRONLY = 0b001,
  O_RDWR = 0b010,
  O_CREAT = 0b0100
};

enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };

/**
 * struct path_search_record - Records the path traversed during file search.
 * @searched_path: Buffer to store the path traversed during the search process.
 * @parent_dir: Pointer to the directory structure representing the direct
 * parent directory of the file or directory being searched.
 * @file_type: Type of the file found. It can be a regular file, directory, or
 *             FT_UNKNOWN if the file is not found.
 *
 * This structure is used to keep track of the path that has been traversed
 * when searching for a file or directory. It stores the path explored, the
 * parent directory at each point of the search, and the type of the file
 * if found. This is useful in file system operations where the path to a
 * file and its parent directory information are required.
 */
struct path_search_record {
  char searched_path[MAX_PATH_LEN];
  struct dir *parent_dir;
  enum file_types file_type;
};

struct stat {
  uint32_t st_ino;
  uint32_t st_size;
  enum file_types st_filetype;
};

void filesys_init();
int32_t sys_open(const char *pathname, uint8_t flag);
int32_t sys_close(int32_t fd);
uint32_t sys_write(int32_t fd, const void *buf, uint32_t count);
int32_t sys_read(int32_t fd, void *buf, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char *pathname);
int32_t sys_mkdir(const char *pathname);
struct dir *sys_opendir(const char *name);
int32_t sys_closedir(struct dir *dir);
struct dir_entry *sys_readdir(struct dir *dir);
void sys_rewinddir(struct dir *dir);
int32_t sys_rmdir(const char *pathname);
char *sys_getcwd(char *buf, uint32_t size);
int32_t sys_chdir(const char *path);
int32_t sys_stat(const char *path, struct stat *buf);

#endif
