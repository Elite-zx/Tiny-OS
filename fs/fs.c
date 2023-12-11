/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#include "fs.h"
#include "bitmap.h"
#include "console.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "io_queue.h"
#include "keyboard.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

extern uint8_t channel_cnt;
extern struct ide_channel channels[2];
extern struct list partition_list;
extern struct dir root_dir;
extern struct file file_table[MAX_FILES_OPEN];

struct partition *cur_part;

/**
 * mount_partition() - Mount a partition by name.
 * @pelem: Pointer to the partition list element.
 * @arg: Argument representing the name of the partition to mount.
 *
 * This function searches for a partition with the given name in the partition
 * list. If found, it mounts the partition by setting 'cur_part' to the found
 * partition. It also reads the super block from the disk into memory,
 * initializes the block bitmap and inode bitmap for the partition, and adds
 * open inodes to the partition's inode list. Returns true to stop the list
 * traversal when the desired partition is mounted, otherwise returns false to
 * continue traversal.
 */
static bool mount_partition(struct list_elem *pelem, const int arg) {
  char *part_name = (char *)arg;
  struct partition *part = elem2entry(struct partition, part_tag, pelem);

  if (!strcmp(part->name, part_name)) {
    cur_part = part;
    struct disk *hd = cur_part->which_disk;

    /*****************************************************************  */
    /* read super block from disk to memory */
    /*****************************************************************  */
    struct super_block *_sup_b_buf =
        (struct super_block *)sys_malloc(SECTOR_SIZE);
    cur_part->sup_b =
        (struct super_block *)sys_malloc(sizeof(struct super_block));

    if (cur_part->sup_b == NULL)
      PANIC("allocate memory failed!");

    memset(_sup_b_buf, 0, SECTOR_SIZE);
    ide_read(hd, cur_part->start_LBA + 1, _sup_b_buf, 1);
    memcpy(cur_part->sup_b, _sup_b_buf, sizeof(struct super_block));

    /** printk("part I mounted:\n"); */
    /** printk("  name: %s\n  root_dir_LBA: 0x%x\n  inode_table_LBA: 0x%x\n  "
     */
    /**        "inode_bitmap_LBA: 0x%x\n  free_blocks_bitmap_LBA: 0x%x\n", */
    /**        cur_part->name, cur_part->sup_b->data_start_LBA, */
    /**        cur_part->sup_b->inode_table_LBA,
     * cur_part->sup_b->inode_bitmap_LBA, */
    /**        cur_part->sup_b->free_blocks_bitmap_LBA); */

    /*****************************************************************  */
    /* read free blocks bitmap from disk to memory */
    /*****************************************************************  */
    cur_part->block_bitmap.bits = (uint8_t *)sys_malloc(
        _sup_b_buf->free_blocks_bitmap_sectors * SECTOR_SIZE);

    if (cur_part->block_bitmap.bits == NULL)
      PANIC("allocate memory failed!");

    cur_part->block_bitmap.bmap_bytes_len =
        _sup_b_buf->free_blocks_bitmap_sectors * SECTOR_SIZE;

    ide_read(hd, _sup_b_buf->free_blocks_bitmap_LBA,
             cur_part->block_bitmap.bits,
             _sup_b_buf->free_blocks_bitmap_sectors);

    /*****************************************************************  */
    /* read inode bitmap from disk to memory */
    /*****************************************************************  */
    cur_part->inode_bitmap.bits =
        (uint8_t *)sys_malloc(_sup_b_buf->inode_bitmap_sectors * SECTOR_SIZE);

    if (cur_part->inode_bitmap.bits == NULL)
      PANIC("allocate memory failed!");

    cur_part->inode_bitmap.bmap_bytes_len =
        _sup_b_buf->inode_bitmap_sectors * SECTOR_SIZE;

    ide_read(hd, _sup_b_buf->inode_bitmap_LBA, cur_part->inode_bitmap.bits,
             _sup_b_buf->inode_bitmap_sectors);

    list_init(&cur_part->open_inodes);
    printk("mount %s done!\n", part->name);
    return true;
  }
  return false;
}

/**
 * partition_format() - Format a partition and create a file system.
 * @part: Pointer to the partition to be formatted.
 *
 * This function formats the specified partition and initializes its file
 * system. It sets up the boot sector, super block, block bitmap, inode bitmap,
 * inode table, and the root directory in the partition. The function writes
 * these structures to the disk, effectively creating a new file system on the
 * partition.
 */
static void partition_format(struct disk *_hd, struct partition *part) {
  /* OBR */
  uint32_t OS_boot_sectors = 1;

  /* the number of sectors occupied by super block */
  uint32_t super_block_sectors = 1;

  /* the number of sectors occupied by inode bitmap */
  uint32_t inode_bitmap_sectors =
      DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);

  /* the number of sectors occupied by inode table */
  uint32_t inode_table_sectors =
      DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE);

  /*****************************************************************  */
  /* calculate the number of sectors occupied by free block bitmap */
  /*****************************************************************  */
  uint32_t used_sectors = OS_boot_sectors + super_block_sectors +
                          inode_bitmap_sectors + inode_table_sectors;
  uint32_t free_sectors = part->sector_cnt - used_sectors;
  /* Initial estimate of sectors for free block bitmap, rounded up to cover all
   * free sectors.  */
  uint32_t free_blocks_bitmap_sectors =
      DIV_ROUND_UP(free_sectors, BITS_PER_SECTOR);
  /* Adjust free sectors by subtracting the initially estimated
   * free_blocks_bitmap sectors.
   */
  uint32_t real_free_blocks_sectors = free_sectors - free_blocks_bitmap_sectors;
  /* Recalculate bitmap sectors with adjusted free sector count for accurate
   * estimation.  */
  free_blocks_bitmap_sectors =
      DIV_ROUND_UP(real_free_blocks_sectors, BITS_PER_SECTOR);

  /*********************************  */
  /* initialize super block  */
  /*********************************  */
  struct super_block _sup_b;
  /* XUN-filesystem type  */
  _sup_b.magic = 0x20011124;
  _sup_b.sector_cnt = part->sector_cnt;
  _sup_b.inode_cnt = MAX_FILES_PER_PART;
  _sup_b.partition_LBA_addr = part->start_LBA;

  _sup_b.free_blocks_bitmap_LBA = part->start_LBA + 2;
  _sup_b.free_blocks_bitmap_sectors = free_blocks_bitmap_sectors;

  _sup_b.inode_bitmap_LBA =
      _sup_b.free_blocks_bitmap_LBA + _sup_b.free_blocks_bitmap_sectors;
  _sup_b.inode_bitmap_sectors = inode_bitmap_sectors;

  _sup_b.inode_table_LBA =
      _sup_b.inode_bitmap_LBA + _sup_b.inode_bitmap_sectors;
  _sup_b.inode_table_sectors = inode_table_sectors;

  _sup_b.data_start_LBA = _sup_b.inode_table_LBA + _sup_b.inode_table_sectors;
  _sup_b.root_inode_NO = 0;
  _sup_b.dir_entry_size = sizeof(struct dir_entry);

  printk("%s info:\n", part->name);
  printk("  magic:0x%x\n  partition_LBA_addr:0x%x\n  total_sectors:0x%x\n  "
         "inode_cnt:0x%x\n  free_blocks_bitmap_LBA:0x%x\n  "
         "free_blocks_bitmap_sectors:0x%x\n  inode_bitmap_LBA:0x%x\n  "
         "inode_bitmap_sectors:0x%x\n  inode_table_LBA:0x%x\n  "
         "inode_table_sectors:0x%x\n  data_start_LBA:0x%x\n",
         _sup_b.magic, _sup_b.partition_LBA_addr, _sup_b.sector_cnt,
         _sup_b.inode_cnt, _sup_b.free_blocks_bitmap_LBA,
         _sup_b.free_blocks_bitmap_sectors, _sup_b.inode_bitmap_LBA,
         _sup_b.inode_bitmap_sectors, _sup_b.inode_table_LBA,
         _sup_b.inode_table_sectors, _sup_b.data_start_LBA);

  /***************************************************************  */
  /* Write the superblock to the first sector of the partition
   * (LBA model, sector number starting from 0)  */
  /***************************************************************  */
  struct disk *hd = part->which_disk;
  ide_write(hd, part->start_LBA + 1, &_sup_b, 1);
  printk("  super_block_LBA:0x%x\n", part->start_LBA + 1);

  uint32_t buf_size =
      (_sup_b.free_blocks_bitmap_sectors > _sup_b.inode_bitmap_sectors
           ? _sup_b.free_blocks_bitmap_sectors
           : _sup_b.inode_bitmap_sectors);

  // clang-format off
  buf_size =(buf_size > _sup_b.inode_table_sectors
             ? buf_size
             : _sup_b.inode_table_sectors)
             * SECTOR_SIZE;
  uint8_t *buf = (uint8_t *)sys_malloc(buf_size);
  // clang-format on

  /*******************************************  */
  /* write free blocks bitmap to disk  */
  /*******************************************  */
  /* Reserve space for the root directory */
  buf[0] |= 0x01;
  /* Calculate the position of the last byte in the bitmap that corresponds to a
   * real free block. */
  uint32_t free_blocks_bitmap_last_byte = real_free_blocks_sectors / 8;
  /* Determine the position of the last effective bit in the bitmap for real
   * free blocks. */
  uint32_t free_blocks_bitmap_last_effective_bit = real_free_blocks_sectors % 8;
  /* Calculate unused space in the last sector of the bitmap. */
  uint32_t bitmap_last_sector_unused_space =
      SECTOR_SIZE - (free_blocks_bitmap_last_byte % SECTOR_SIZE);
  /* Initialize the unused part of the last sector in the bitmap to 0xff to
   * mark it as used, which means no longer usable. */
  memset(&buf[free_blocks_bitmap_last_byte], 0xff,
         bitmap_last_sector_unused_space);
  uint8_t bit_idx = 0;
  while (bit_idx <= free_blocks_bitmap_last_effective_bit) {
    buf[free_blocks_bitmap_last_byte] &= ~(1 << bit_idx++);
  }
  ide_write(hd, _sup_b.free_blocks_bitmap_LBA, buf,
            _sup_b.free_blocks_bitmap_sectors);

  /*********************************  */
  /* initialize inode bitmap  */
  /*********************************  */
  memset(buf, 0, buf_size);
  buf[0] |= 0x01;
  ide_write(hd, _sup_b.inode_bitmap_LBA, buf, _sup_b.inode_bitmap_sectors);

  /*********************************  */
  /* initialize inode table  */
  /*********************************  */
  memset(buf, 0, buf_size);
  /* initialize root directory inode in inode table  */
  struct inode *i = (struct inode *)buf;
  i->i_NO = 0;
  i->i_size = _sup_b.dir_entry_size * 2;
  i->i_blocks[0] = _sup_b.data_start_LBA;
  ide_write(hd, _sup_b.inode_table_LBA, buf, _sup_b.inode_table_sectors);

  /*********************************  */
  /* initialize directory '.' and '..' for root directory */
  /*********************************  */
  memset(buf, 0, buf_size);
  struct dir_entry *de = (struct dir_entry *)buf;
  /* current directory '.'  */
  memcpy(de->filename, ".", 1);
  de->f_type = FT_DIRECTORY;
  de->i_NO = 0;
  de++;
  /* parent directory '..' */
  memcpy(de->filename, "..", 2);
  de->f_type = FT_DIRECTORY;
  de->i_NO = 0;

  ide_write(hd, _sup_b.data_start_LBA, buf, 1);

  printk("  root_dir_LBA:0x%x\n", _sup_b.data_start_LBA);
  printk("  %s format done\n", part->name);
}

/**
 * filesys_init() - Initializes the file system.
 *
 * Scans all the hard drives on the system to search for existing file systems.
 * If a file system is not found on a partition, it formats the partition and
 * creates a new file system. The function skips over raw disks (like hd60M.img)
 * and only processes valid partitions. It supports only its own file system
 * type identified by the magic number 0x20011124.
 *
 * It also sets a default partition to operate on and opens the root directory
 * of the current partition. Finally, it initializes the global file table
 * for managing open files.
 */
void filesys_init() {
  uint8_t channel_NO = 0, part_idx = 0;
  uint8_t dev_NO;
  /* _sup_b_buf is the buffer used to store super_block(which is read from disk)
   */
  struct super_block *_sup_b_buf =
      (struct super_block *)sys_malloc(SECTOR_SIZE);
  if (_sup_b_buf == NULL)
    PANIC("allocate memory failed!");
  printk("searching filesystem......\n");
  while (channel_NO < channel_cnt) {
    /*  traverse channels  */
    dev_NO = 0;
    while (dev_NO < 2) {
      /*  traverse disks  */
      if (dev_NO == 0) {
        /* skip over main disk---hd60M.img  */
        dev_NO++;
        continue;
      }
      struct disk *hd = &channels[channel_NO].devices[dev_NO];
      struct partition *part = hd->prim_parts;
      while (part_idx < 12) {
        /*  traverse partitions  */
        if (part_idx == 4) {
          /* 4 primary partitions have been processed, now process logical
           * partitions ^_^  */
          part = hd->logic_parts;
        }
        /* partition exists or not  */
        if (part->sector_cnt != 0) {
          memset(_sup_b_buf, 0, SECTOR_SIZE);
          /* '+1' here is to skip over the OBR sector and read super block which
           * located in the second sector of partition*/
          ide_read(hd, part->start_LBA + 1, _sup_b_buf, 1);
          if (_sup_b_buf->magic == 0x20011124) {
            printk("%s has filesystem\n", part->name);
          } else {
            printk("fromatting %s's partition %s......\n", hd->name,
                   part->name);
            partition_format(hd, part);
          }
        }
        part_idx++;
        part++;
      }
      dev_NO++;
    }
    channel_NO++;
  }
  sys_free(_sup_b_buf);

  char default_part[8] = "sdb1";
  list_traversal(&partition_list, mount_partition, (int)default_part);

  open_root_dir(cur_part);
  uint32_t fd_idx = 0;
  while (fd_idx < MAX_FILES_OPEN) {
    file_table[fd_idx++].fd_inode = NULL;
  }
}

/**
 * path_parse() - Extract the top-level name from a path.
 * @pathname: The full path to parse.
 * @name_store: Buffer to store the extracted name.
 *
 * This function parses a pathname and extracts the highest level name
 * from it. It skips leading '/' characters and copies the first part
 * of the pathname until the next '/' or the end of the string. The
 * extracted name is stored in 'name_store'. The function returns a
 * pointer to the remainder of the pathname, or NULL if the end of the
 * pathname has been reached.
 */
static char *path_parse(char *pathname, char *name_buf) {
  if (pathname[0] == '/') {
    /* skip over the begining '/',  eg: ///home/elite-zx -> home/elite-zx  */
    while (*(++pathname) == '/')
      ;
  }
  /* eg: pathname--- home/elite-zx -> /elite-zx, name_buf --- home  */
  while (*pathname != '/' && *pathname != '\0') {
    *name_buf++ = *pathname++;
  }
  if (pathname[0] == '\0') {
    /* path parse done ! */
    return NULL;
  }

  return pathname;
}

/**
 * path_depth_cnt() - Count the depth of a given path.
 * @pathname: The path to count the depth of.
 *
 * Returns the number of levels in a given pathname. For example, a path
 * like "/a/b/c" has a depth of 3. The function iteratively parses the path
 * to count the number of directories (or levels) it contains.
 */
int32_t path_depth_cnt(char *pathname) {
  ASSERT(pathname != NULL);
  char *p = pathname;
  char name_buf[MAX_FILE_NAME_LEN];
  uint32_t depth = 0;

  p = path_parse(p, name_buf);
  while (*name_buf) {
    depth++;
    memset(name_buf, 0, MAX_FILE_NAME_LEN);
    if (p) {
      p = path_parse(p, name_buf);
    }
  }
  return depth;
}

/**
 * search_file() - Search for a file or directory in a partition.
 * @pathname: The name of the file or directory to search for.
 * @searched_record: Pointer to a path_search_record structure where the search
 *                   results will be stored.
 *
 * Searches for a file or directory in the file system. If found, the function
 * returns the inode number of the file or directory and fills the
 * 'searched_record' structure with the details of the search, including the
 * parent directory and the type of file found. If the file or directory is not
 * found, the function returns -1.
 */
static int search_file(const char *pathname,
                       struct path_search_record *searched_record) {
  /** target file is root directory, so no searching process  */
  if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") ||
      !strcmp(pathname, "/..")) {
    searched_record->searched_path[0] = 0;
    searched_record->parent_dir = &root_dir;
    searched_record->file_type = FT_DIRECTORY;
    /* root_dir._inode->i_NO is 0  */
    return 0;
  }
  uint32_t path_len = strlen(pathname);
  ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);

  char *sub_path = (char *)pathname;
  char name_buf[MAX_FILE_NAME_LEN] = {0};
  /* Search from root directory  */
  struct dir *parent_dir = &root_dir;
  searched_record->parent_dir = parent_dir;
  struct dir_entry dir_e;
  searched_record->file_type = FT_UNKNOWN;
  uint32_t parent_inode_NO = 0;

  /* Get the top directory in the path */
  sub_path = path_parse(sub_path, name_buf);

  while (*name_buf) {
    ASSERT(strlen(searched_record->searched_path) < 512);
    strcat(searched_record->searched_path, "/");
    strcat(searched_record->searched_path, name_buf);

    /* search file (stored in name_buf) in directory (parent_dir) by invoking
     * function search_dir_entry, which defined in file dir.c. the corresponding
     * dir entry store in dir_e */
    if (search_dir_entry(cur_part, parent_dir, name_buf, &dir_e)) {
      memset(name_buf, 0, MAX_FILE_NAME_LEN);
      if (sub_path) {
        /* go on parsing */
        sub_path = path_parse(sub_path, name_buf);
      }

      if (dir_e.f_type == FT_DIRECTORY) {
        parent_inode_NO = parent_dir->_inode->i_NO;
        dir_close(parent_dir);
        /* update parent_dir  */
        parent_dir = dir_open(cur_part, dir_e.i_NO);
        searched_record->parent_dir = parent_dir;
        continue;
      } else if (dir_e.f_type == FT_REGULAR) {
        /* I found you !  */
        searched_record->file_type = FT_REGULAR;
        return dir_e.i_NO;
      }
    } else {
      /* Target directory entry not found in current file  */
      return -1;
    }
  }
  dir_close(searched_record->parent_dir);
  searched_record->parent_dir = dir_open(cur_part, parent_inode_NO);
  searched_record->file_type = FT_DIRECTORY;
  return dir_e.i_NO;
}

/**
 * sys_open() - Open or create a file.
 * @pathname: Path of the file to be opened.
 * @flags: Flags specifying the file access mode and other settings.
 *
 * If the file specified by pathname exists, the function opens it with
 * the access mode specified in flags. If the file does not exist and
 * O_CREAT flag is specified, the file is created. This function can't
 * be used to open directories. For directories, opendir() should be used.
 *
 * The function returns a file descriptor on success, and -1 on failure.
 *
 * Context: Works within the context of the calling process. Handles path
 *          traversal and checks if the file or directory exists at the given
 * path.
 * Return: File descriptor on success, -1 on failure.
 */
int32_t sys_open(const char *pathname, uint8_t flag) {
  if (pathname[strlen(pathname) - 1] == '/') {
    printk("sys_open: Can't open a directory %s\n", pathname);
    return -1;
  }
  ASSERT(flag < 0b1000);
  int32_t fd = -1;

  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));

  uint32_t pathname_depth = path_depth_cnt((char *)pathname);

  int inode_NO = search_file(pathname, &searched_record);
  bool found = inode_NO != -1 ? true : false;

  if (searched_record.file_type == FT_DIRECTORY) {
    printk(
        "sys_open: Can't open a directory with open(), user opendir instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
  if (path_searched_depth != pathname_depth) {
    printk(
        "sys_open: Cannot access %s: Not a directory, subpath %s is't exist\n",
        pathname, searched_record.searched_path);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  if (!found && !(flag & O_CREAT)) {
    printk("sys_open: In path %s,file %s is't exist\n",
           searched_record.searched_path,
           (strrchr(searched_record.searched_path, '/') + 1));
    dir_close(searched_record.parent_dir);
    return -1;
  } else if (found && flag & O_CREAT) {
    printk("%s has already exist!\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  switch (flag & O_CREAT) {
  case O_CREAT:
    /* file does not exists  */
    printk("creating file\n");
    fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1),
                     flag);
    dir_close(searched_record.parent_dir);
    break;
  default:
    /* file exists  */
    fd = file_open(inode_NO, flag);
  }
  return fd;
}

/**
 * fd_local_2_global() - Convert a local file descriptor to a global file table
 * index.
 * @local_fd: The local file descriptor.
 *
 * Converts a file descriptor that is local to the current process (such as
 * those obtained from sys_open) into a global file descriptor index used in the
 * system-wide file table. This function is used to map the process's own file
 * descriptor space to the global file descriptor space, allowing the system to
 * reference the correct file structure in the global file table.
 *
 * It's important to ensure that the local file descriptor is valid and
 * currently in use by the process. The function asserts that the global file
 * descriptor index is within the valid range of the file table.
 *
 * Context: Should be called within the context of the process that owns the
 * local file descriptor.
 * Return: The global file table index corresponding to
 * the local file descriptor.
 */
static uint32_t fd_local_2_global(uint32_t local_fd_idx) {
  struct task_struct *cur = running_thread();
  int32_t global_fd_idx = cur->fd_table[local_fd_idx];
  ASSERT(global_fd_idx >= 0 && global_fd_idx < MAX_FILES_OPEN);
  return (uint32_t)global_fd_idx;
}

/**
 * sys_close() - Close a file.
 * @fd: The file descriptor of the file to be closed.
 *
 * Closes the file associated with the file descriptor fd. This function
 * is intended to be called for file descriptors that refer to files,
 * not directories. The file descriptor fd is removed from the calling
 * process's file descriptor table and is made available for future
 * sys_open calls.
 *
 * This function does not work for standard input, output, and error
 * (file descriptors 0, 1, and 2). Attempts to close these file descriptors
 * will have no effect and the function will return -1.
 *
 * The function returns 0 on successful closing of the file, and -1 on failure.
 *
 * Context: Works within the context of the calling process. Closes the file
 *          if it's currently opened by the process.
 * Return: 0 on success, -1 on failure.
 */
int32_t sys_close(int32_t fd) {
  int32_t ret = -1;
  if (fd > 2) {
    uint32_t _fd = fd_local_2_global(fd);
    ret = file_close(&file_table[_fd]);
    running_thread()->fd_table[fd] = -1;
  }
  return ret;
}

/**
 * sys_write() - Write data to a file or standard output.
 * @fd: File descriptor of the file or standard output.
 * @buf: Buffer containing the data to be written.
 * @count: Number of bytes to write.
 *
 * Writes 'count' bytes of data from 'buf' to the file associated with 'fd'.
 * If 'fd' is a standard output file descriptor (STDOUT_NO), the function
 * writes data to the console. Otherwise, it writes to the file specified
 * by 'fd'. This function handles writing to files with write permissions,
 * and it ensures that the file is open in a mode that allows writing
 * (O_WRONLY or O_RDWR).
 *
 * Return: Number of bytes written on success, or -1 on failure.
 *         For STDOUT_NO, always returns the 'count' of bytes.
 *
 * Note: 'fd' should be a valid file descriptor. If 'fd' is negative or
 *       if the file is not open in a suitable mode for writing, the
 *       function returns -1.
 */
uint32_t sys_write(int32_t fd, const void *buf, uint32_t count) {
  if (fd < 0) {
    printk("sys_write: fd error\n");
    return -1;
  }

  if (fd == STDOUT_NO) {
    char io_buf[1024] = {0};
    memcpy(io_buf, buf, count);
    console_put_str(io_buf);
    return count;
  }

  uint32_t _fd = fd_local_2_global(fd);
  struct file *wr_file = &file_table[_fd];
  if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
    uint32_t bytes_written = file_write(wr_file, buf, count);
    return bytes_written;
  } else {
    console_put_str("sys_write: not allowed to write file without flag "
                    "O_WRONLY or O_RDWR\n");
    return -1;
  }
}

/**
 * sys_read() - Reads data from a file.
 * @fd: File descriptor of the file to read from.
 * @buf: Buffer to store the read data.
 * @count: Number of bytes to read.
 *
 * This function reads 'count' bytes of data from the file identified by 'fd'
 * into the buffer 'buf'. It handles the conversion of the local file descriptor
 * to a global file table index and delegates the reading operation to the
 * file_read function.
 *
 * Return: The number of bytes successfully read, or -1 if an error occurred.
 */
int32_t sys_read(int32_t fd, void *buf, uint32_t count) {
  ASSERT(buf != NULL);
  int ret_val = -1;
  if (fd < 0 || fd == STDOUT_NO || fd == STDERR_NO) {
    printk("sys_write: fd error\n");
  } else if (fd == STDIN_NO) {
    char *buffer = buf;
    uint32_t bytes_read = 0;
    while (bytes_read < count) {
      /* get char from keyboard io buffer  */
      *buffer = ioq_getchar(&kbd_circular_buf);
      bytes_read++;
      buffer++;
    }
    ret_val = (bytes_read == 0) ? -1 : (int32_t)bytes_read;
  } else {
    uint32_t _fd = fd_local_2_global(fd);
    ret_val = file_read(&file_table[_fd], buf, count);
  }
  return ret_val;
}

/**
 * sys_lseek() - Changes the file offset for read/write operations.
 * @fd: File descriptor of the file.
 * @offset: The new offset based on the 'whence' parameter.
 * @whence: Position used as reference for the offset (SEEK_SET, SEEK_CUR,
 * SEEK_END).
 *
 * This function adjusts the file offset for the file represented by 'fd'.
 * The new position is calculated based on 'offset' and 'whence'. SEEK_SET sets
 * the offset relative to the start of the file, SEEK_CUR sets it relative to
 * the current position, and SEEK_END sets it relative to the end of the file.
 *
 * Return: The new file offset on success, or -1 if an error occurred.
 */
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
  if (fd < 0) {
    printk("sys_lseek: fd error\n");
    return -1;
  }

  ASSERT(whence < 4);
  uint32_t _fd = fd_local_2_global(fd);
  struct file *pf = &file_table[_fd];
  int32_t new_fd_pos = 0;
  int32_t file_size = pf->fd_inode->i_size;
  switch (whence) {
  case SEEK_SET:
    new_fd_pos = offset;
    break;
  case SEEK_CUR:
    new_fd_pos = (int32_t)pf->fd_pos + offset;
    break;
  case SEEK_END:
    new_fd_pos = file_size + offset;
  }

  /* beyond the range of file  */
  if (new_fd_pos < 0 || new_fd_pos > (file_size - 1))
    return -1;

  pf->fd_pos = new_fd_pos;
  return pf->fd_pos;
}

/**
 * sys_unlink() - Deletes a file from the filesystem.
 * @pathname: Path to the file to be deleted.
 *
 * This function deletes the file specified by 'pathname'. It first checks if
 * the file exists and is not a directory. The function then removes the
 * directory entry corresponding to the file and releases its inode. If the file
 * is currently open, the deletion will fail.
 *
 * Note: Directories cannot be deleted with this function.
 *
 * Return: 0 on successful deletion, -1 on failure.
 */
int32_t sys_unlink(const char *pathname) {
  ASSERT(strlen(pathname) < MAX_PATH_LEN);
  /******** search pathname in current partition ********/
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_NO = search_file(pathname, &searched_record);
  ASSERT(inode_NO != 0);
  if (inode_NO == -1) {
    printk("file %s not found!\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }
  if (searched_record.file_type == FT_DIRECTORY) {
    printk("can't delete a directory with unlink() ,use rmdir() instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  /* check whether the file `pathname` is in the list of open files
   * (file_table) */
  uint32_t file_idx = 0;
  while (file_idx < MAX_FILES_OPEN) {
    if (file_table[file_idx].fd_inode != NULL &&
        (uint32_t)inode_NO == file_table[file_idx].fd_inode->i_NO) {
      break;
    }
    file_idx++;
  }
  if (file_idx < MAX_FILES_OPEN) {
    /* the file is open (which means in use) and cannot be deleted  */
    dir_close(searched_record.parent_dir);
    printk("file %s is in use, not allow to delete!\n", pathname);
    return -1;
  }

  /* the file is not open and can be deleted  */
  ASSERT(file_idx == MAX_FILES_OPEN);
  void *io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    dir_close(searched_record.parent_dir);
    printk("sys_unlink: sys_malloc for io_buf failed\n");
    return -1;
  }

  struct dir *parent_dir = searched_record.parent_dir;
  delete_dir_entry(cur_part, parent_dir, inode_NO, io_buf);

  inode_release(cur_part, inode_NO);
  sys_free(io_buf);
  dir_close(searched_record.parent_dir);
  return 0;
}

/**
 * sys_mkdir() - Creates a new directory.
 * @pathname: Path where the new directory will be created.
 *
 * This function creates a new directory in the filesystem at the specified
 * 'pathname'. It handles various checks such as the existence of the directory,
 * allocation of an inode for the new directory, creation of '.' and '..'
 * entries, and updates parent directory's inode and directory entries.
 *
 * Return: 0 on success, -1 on failure.
 * Rollback mechanism in place to undo actions in case of failure at any step.
 */
int32_t sys_mkdir(const char *pathname) {
  uint32_t rollback_action = 0;

  void *io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    printk("sys_mkdir: sys_malloc for io_buf failed\n");
    return -1;
  }
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_NO = -1;
  inode_NO = search_file(pathname, &searched_record);
  if (inode_NO != -1) {
    printk("sys_mkdir: directory %s already exists!\n");
    rollback_action = 2;
    goto rollback;
  } else {
    /* Check whether it is not found because the intermediate directory does
     * not exist */
    uint32_t pathname_depth = path_depth_cnt((char *)pathname);
    uint32_t path_searched_depth =
        path_depth_cnt(searched_record.searched_path);
    if (pathname_depth != path_searched_depth) {
      printk("sys_mkdir: cannot access %s: subpath %s is't "
             "exist\n",
             pathname, searched_record.searched_path);
      rollback_action = 2;
      goto rollback;
    }
  }

  struct dir *parent_dir = searched_record.parent_dir;
  char *dirname = strrchr(searched_record.searched_path, '/') + 1;

  /******** create inode ********/
  int new_inode_NO = inode_bitmap_alloc(cur_part);
  if (new_inode_NO == -1) {
    printk("sys_mkdir: allocate inode failed\n");
    rollback_action = 2;
    goto rollback;
  }
  struct inode new_dir_inode;
  inode_init(new_inode_NO, &new_dir_inode);

  /******** allocate a block to this directory ********/
  uint32_t block_bitmap_idx = 0;
  int32_t block_LBA = -1;
  block_LBA = block_bitmap_alloc(cur_part);
  if (block_LBA == -1) {
    printk("sys_mkdir: block_bitmap_alloc for create directory failed\n");
    rollback_action = 1;
    goto rollback;
  }
  new_dir_inode.i_blocks[0] = block_LBA;
  /* synchronize block bitmap to disk  */
  block_bitmap_idx = block_LBA - cur_part->sup_b->data_start_LBA;
  ASSERT(block_bitmap_idx != 0);
  bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

  /******** create dir_entry '.' and '..' ********/
  memset(io_buf, 0, SECTOR_SIZE * 2);
  struct dir_entry *de = (struct dir_entry *)io_buf;
  /* current directory '.'  */
  memcpy(de->filename, ".", 1);
  de->f_type = FT_DIRECTORY;
  de->i_NO = new_inode_NO;
  de++;
  /* parent directory '..' */
  memcpy(de->filename, "..", 2);
  de->f_type = FT_DIRECTORY;
  de->i_NO = parent_dir->_inode->i_NO;
  ide_write(cur_part->which_disk, new_dir_inode.i_blocks[0], io_buf, 1);
  new_dir_inode.i_size += 2 * cur_part->sup_b->dir_entry_size;

  /******** add directory entry to parent directory ********/
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));
  create_dir_entry(dirname, new_inode_NO, FT_DIRECTORY, &new_dir_entry);
  memset(io_buf, 0, SECTOR_SIZE * 2);
  if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
    printk("sys_mkdir: sync_dir_entry to disk failed\n");
    rollback_action = 1;
    goto rollback;
  }
  /******** synchronize inode table and inode bitmap to disk ********/
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, parent_dir->_inode, io_buf);
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, &new_dir_inode, io_buf);
  bitmap_sync(cur_part, new_inode_NO, INODE_BITMAP);

  sys_free(io_buf);
  dir_close(parent_dir);
  return 0;

/******** perform rollback  (AKA exception handling) ********/
rollback:
  switch (rollback_action) {
  case 1:
    bitmap_set(&cur_part->inode_bitmap, inode_NO, 0);
  case 2:
    dir_close(searched_record.parent_dir);
    break;
  }
  sys_free(io_buf);
  return -1;
}

/**
 * sys_opendir() - Opens a directory.
 * @name: Path of the directory to open.
 *
 * Opens a directory for reading its contents. This function checks if the
 * directory exists and is not a regular file. It then returns a directory
 * structure which can be used to read the directory entries.
 *
 * Return: Pointer to the directory structure if successful, NULL if failed.
 */
struct dir *sys_opendir(const char *name) {
  ASSERT(strlen(name) < MAX_PATH_LEN);

  /******** Check if file 'name' is root_dir ********/

  /* both '/', '/.' and '/..' are root dir */
  if (name[0] == '/') {
    if (name[1] == 0 || (name[1] == '.' && name[2] == 0) ||
        (name[1] == '.' && name[2] == '.' && name[3] == 0)) {
      return &root_dir;
    }
  }

  /******** Check if file 'name' exists ********/
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  struct dir *target_dir_ptr = NULL;
  int inode_NO = search_file(name, &searched_record);
  if (inode_NO == -1) {
    printk("In %s, subpath %s not exitts\n", name,
           searched_record.searched_path);
  } else {
    if (searched_record.file_type == FT_REGULAR) {
      printk("%s is regular file!\n", name);
    } else if (searched_record.file_type == FT_DIRECTORY) {
      target_dir_ptr = dir_open(cur_part, inode_NO);
    }
  }
  dir_close(searched_record.parent_dir);
  return target_dir_ptr;
}

/**
 * sys_closedir() - Closes an open directory.
 * @dir: Pointer to the directory structure.
 *
 * Closes a directory that was previously opened by sys_opendir. This involves
 * freeing up resources associated with the directory structure.
 *
 * Return: 0 on successful close, -1 if an error occurred or if 'dir' is NULL.
 */
int32_t sys_closedir(struct dir *dir) {
  int32_t ret = -1;
  if (dir != NULL) {
    dir_close(dir);
    ret = 0;
  }
  return ret;
}

/**
 * sys_readdir() - Reads a directory entry from a directory.
 * @dir: The directory from which to read the entry.
 *
 * Reads and returns the next directory entry from the directory specified by
 * 'dir'. The reading position is maintained internally in the directory's
 * dir_pos field. When the end of the directory is reached or in case of an
 * error, NULL is returned.
 *
 * Return: Pointer to the next directory entry on success, NULL at the end of
 * the directory or on error.
 */
struct dir_entry *sys_readdir(struct dir *dir) {
  ASSERT(dir != NULL);
  return dir_read(dir);
}

/**
 * sys_rewinddir() - Resets the directory pointer.
 * @dir: Pointer to the directory.
 *
 * This function resets the directory's pointer (dir_pos) to 0,
 * which means the next read from the directory will start from the beginning.
 */
void sys_rewinddir(struct dir *dir) { dir->dir_pos = 0; }

/**
 * sys_rmdir() - Removes an empty directory.
 * @pathname: Path of the directory to be removed.
 *
 * Removes a directory specified by 'pathname'. The directory must be empty
 * and not in use. This function handles the deletion of the directory's
 * inode and its entry in the parent directory.
 *
 * Return: 0 on success, -1 on failure.
 */
int32_t sys_rmdir(const char *pathname) {
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));

  int inode_NO = search_file(pathname, &searched_record);
  /* make sure that dir to delete is not root directory  */
  ASSERT(inode_NO != 0);

  int ret_val = -1;
  if (inode_NO == -1) {
    printk("In %s, subpath %s not exist\n", pathname,
           searched_record.searched_path);
  } else {
    /* dir to delete exists  */
    if (searched_record.file_type == FT_REGULAR) {
      printk("%s is regular file\n", pathname);
    } else {
      struct dir *dir = dir_open(cur_part, inode_NO);
      if (!dir_is_empty(dir)) {
        printk("dir %s is not empty, it is not allowed to delete a nonempty "
               "directory!\n",
               pathname);
      } else {
        if (!dir_remove(searched_record.parent_dir, dir)) {
          ret_val = 0;
        }
      }
      dir_close(dir);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret_val;
}

/**
 * get_parent_dir_inode_nr() - Gets the inode number of the parent directory.
 * @child_dir_inode_NO: Inode number of the child directory.
 * @io_buf: Buffer for reading data from the disk.
 *
 * Reads the ".." entry in the child directory to get the inode number of
 * its parent directory.
 *
 * Return: Inode number of the parent directory.
 */
static uint32_t get_parent_dir_inode_NO(uint32_t child_dir_inode_NO,
                                        void *io_buf) {
  /******** get parent directory inode number from the directory entry
   * '..' of child_dir ********/
  struct inode *child_dir_inode = inode_open(cur_part, child_dir_inode_NO);
  /* read the first data block of child_dir_inode from disk to get the address
   * of dir_entry '..'  */
  uint32_t block_LBA = child_dir_inode->i_blocks[0];
  ASSERT(block_LBA >= cur_part->sup_b->data_start_LBA);
  ide_read(cur_part->which_disk, block_LBA, io_buf, 1);
  inode_close(child_dir_inode);

  struct dir_entry *dir_entry_iter = (struct dir_entry *)io_buf;
  ASSERT(dir_entry_iter->i_NO < 4096 &&
         dir_entry_iter[1].f_type == FT_DIRECTORY);

  return dir_entry_iter[1].i_NO;
}

/**
 * get_child_dir_name() - Finds the name of a child directory in a parent
 * directory.
 * @p_inode_NO: Inode number of the parent directory.
 * @c_inode_NO: Inode number of the child directory.
 * @path: Buffer to store the name of the child directory.
 * @io_buf: Buffer for disk IO operations.
 *
 * Searches the parent directory to find the entry of the child directory and
 * retrieves its name.
 *
 * Return: 0 on success, -1 on failure.
 */
static int get_child_dir_name(uint32_t p_inode_NO, uint32_t c_inode_NO,
                              char *path, void *io_buf) {
  struct inode *parent_dir_inode = inode_open(cur_part, p_inode_NO);
  /******** fill all_blocks_addr with the addresses of all blocks of parent
   * directory file ********/
  uint8_t block_idx = 0;
  uint32_t all_blocks_addr[140], block_cnt = 12;
  while (block_idx < 12) {
    all_blocks_addr[block_idx] = parent_dir_inode->i_blocks[block_idx];
    block_idx++;
  }

  if (parent_dir_inode->i_blocks[12] != 0) {
    ide_read(cur_part->which_disk, parent_dir_inode->i_blocks[12],
             all_blocks_addr + 12, 1);
    block_cnt += 128;
  }
  inode_close(parent_dir_inode);

  /******** traverse all dir entries of all blocks of the parent dir to find the
   * target subdirectory file
   * ********/
  struct dir_entry *dir_entry_base = (struct dir_entry *)io_buf;
  uint32_t _dir_entry_size = cur_part->sup_b->dir_entry_size;
  uint32_t max_dir_entry_per_sector = SECTOR_SIZE / _dir_entry_size;
  block_idx = 0;
  while (block_idx < block_cnt) {
    if (all_blocks_addr[block_idx] != 0) {
      ide_read(cur_part->which_disk, all_blocks_addr[block_idx], io_buf, 1);
      uint8_t dir_entry_idx = 0;
      while ((dir_entry_idx < max_dir_entry_per_sector)) {
        if ((dir_entry_base + dir_entry_idx)->i_NO == c_inode_NO) {
          strcat(path, "/");
          strcat(path, (dir_entry_base + dir_entry_idx)->filename);
          return 0;
        }
        /* next dir entry within the same block  */
        dir_entry_idx++;
      }
    }
    /* block is empty, move to next  */
    block_idx++;
  }
  /* target subdirectory file is not found  */
  return -1;
}

/**
 * sys_getcwd() - Gets the current working directory.
 * @buf: Buffer where the absolute path of the current working directory will be
 * stored.
 * @size: Size of the buffer 'buf'.
 *
 * Writes the absolute path of the current working directory to 'buf'.
 * The path is constructed by moving from the current directory upwards
 * to the root directory and reversing the constructed path.
 *
 * Return: The pointer to 'buf' containing the absolute path, or NULL on
 * failure.
 */
char *sys_getcwd(char *buf, uint32_t size) {
  ASSERT(buf != NULL);
  void *io_buf = sys_malloc(SECTOR_SIZE);
  if (io_buf == NULL)
    return NULL;

  struct task_struct *cur_thread = running_thread();
  int32_t parent_inode_NO = 0;
  int32_t child_dir_inode_NO = cur_thread->cwd_inode_NO;
  ASSERT(child_dir_inode_NO >= 0 && child_dir_inode_NO < 4096);

  if (child_dir_inode_NO == 0) {
    /* cwd is root directory, do nothing */
    buf[0] = '/';
    buf[1] = 0;
    return buf;
  }

  memset(buf, 0, size);
  char full_path_reverse[MAX_PATH_LEN] = {0};

  /******** find the parent directory layer by layer from bottom to top
   * ********/
  while (((child_dir_inode_NO != 0))) {
    /**** Get the directory name of the directory where the file is located
     * ****/
    parent_inode_NO = get_parent_dir_inode_NO(child_dir_inode_NO, io_buf);
    if (get_child_dir_name(parent_inode_NO, child_dir_inode_NO,
                           full_path_reverse, io_buf) == -1) {
      /* the name of directory is not found  */
      sys_free(io_buf);
      return NULL;
    }
    child_dir_inode_NO = parent_inode_NO;
  }

  /******** reverse the full path, store the result in buf ********/
  ASSERT(strlen(full_path_reverse) <= size);
  char *last_slash;
  while (((last_slash = strrchr(full_path_reverse, '/')))) {
    uint16_t len = strlen(buf);
    strcpy(buf + len, last_slash);
    *last_slash = 0;
  }
  sys_free(io_buf);
  return buf;
}

/**
 * sys_chdir() - Changes the current working directory.
 * @path: The path of the new directory to change to.
 *
 * Changes the current working directory of the running thread to the directory
 * specified by 'path'. The function first verifies that the path exists, is a
 * directory, and then updates the current working directory.
 *
 * Return: 0 on success, -1 on failure.
 */
int32_t sys_chdir(const char *path) {
  int32_t ret = -1;
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_NO = search_file(path, &searched_record);
  if (inode_NO != -1) {
    if (searched_record.file_type == FT_DIRECTORY) {
      running_thread()->cwd_inode_NO = inode_NO;
      ret = 0;
    } else {
      printk("sys_chdir: %s is regular file or other!\n", path);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

/**
 * sys_stat() - Retrieves the status of a file or directory.
 * @path: The file or directory path.
 * @buf: A pointer to the struct stat where the status information will be
 * stored.
 *
 * Fills the 'buf' structure with information about the file or directory
 * specified by 'path'. This information includes the file type, inode number,
 * and file size.
 *
 * Return: 0 on success, -1 if the file or directory does not exist.
 */
int32_t sys_stat(const char *path, struct stat *buf) {
  if (!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")) {
    buf->st_filetype = FT_DIRECTORY;
    buf->st_ino = 0;
    buf->st_size = root_dir._inode->i_size;
    return 0;
  }

  int32_t ret_val = -1;
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_NO = search_file(path, &searched_record);
  /* target file exists  */
  if (inode_NO != -1) {
    struct inode *target_inode = inode_open(cur_part, inode_NO);
    buf->st_size = target_inode->i_size;
    buf->st_filetype = searched_record.file_type;
    buf->st_ino = inode_NO;
    inode_close(target_inode);
    ret_val = 0;
  } else {
    printk("sys_stat: %s not found\n", path);
  }
  dir_close(searched_record.parent_dir);
  return ret_val;
}
