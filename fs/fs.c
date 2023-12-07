/*
 * Author: Xun Morris
 * Time: 2023-12-04
 */
#include "fs.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"

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

    printk("part I mounted:\n");
    printk("  name: %s\n  root_dir_LBA: 0x%x\n", cur_part->name,
           cur_part->sup_b->data_start_LBA);

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
      memset(name_buf, 0, 0);
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
    printk("creating file\n");
    fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1),
                     flag);
    dir_close(searched_record.parent_dir);
  }
  return fd;
}
