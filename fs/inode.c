/*
 * Author: Zhang Xun
 * Time: 2023-12-05
 */
#include "inode.h"
#include "bitmap.h"
#include "debug.h"
#include "file.h"
#include "ide.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

extern struct partition *cur_part;
/**
 * struct inode_position - Store the position of an inode.
 * @is_inode_cross_sectors: Indicates if the inode spans across two sectors.
 * @sector_LBA: The sector number where the inode is located.
 * @offset_in_sector: The byte offset of the inode within its sector.
 *
 * This structure is used to keep track of an inode's specific location on disk,
 * including whether it spans across two sectors and its exact position in terms
 * of sector number and byte offset within that sector.
 */
struct inode_position {
  bool is_inode_cross_sectors;
  uint32_t sector_LBA;
  uint32_t offset_in_sector;
};

/**
 * inode_locate() - Locate the sector and offset of an inode in a partition.
 * @part: Partition in which to find the inode.
 * @inode_no: The number of the inode to locate.
 * @inode_pos: Structure where the position of the inode will be stored.
 *
 * This function calculates the exact location of an inode within a partition,
 * given the inode number. It fills in the provided inode_position structure
 * with the sector number and byte offset of the inode. It also determines
 * whether the inode spans two sectors.
 */
static void inode_locate(struct partition *part, uint32_t inode_NO,
                         struct inode_position *inode_pos) {
  ASSERT(inode_NO < 4096);
  uint32_t _inode_table_LBA = part->sup_b->inode_table_LBA;

  uint32_t inode_size = sizeof(struct inode);
  uint32_t offset_bytes = inode_NO * inode_size;
  uint32_t offset_sectors = offset_bytes / 512;
  uint32_t _offset_in_sector = offset_bytes % 512;

  if ((512 - _offset_in_sector) < inode_size) {
    inode_pos->is_inode_cross_sectors = true;
  } else {
    inode_pos->is_inode_cross_sectors = false;
  }

  inode_pos->sector_LBA = _inode_table_LBA + offset_sectors;
  inode_pos->offset_in_sector = _offset_in_sector;
}

/**
 * inode_sync() - Writes an inode to a partition.
 * @part: Pointer to the partition structure where the inode resides.
 * @inode: Pointer to the inode structure to be written to the disk.
 * @io_buf: Buffer used for disk I/O operations.
 *
 * This function synchronizes an inode from memory to disk. It writes the
 * inode's data to the specified partition 'part'. The function locates the disk
 * sector(s) where the inode should be written, prepares a clean copy of the
 * inode for disk storage (excluding certain memory-only fields), and then
 * writes it to the disk. It handles both situations where the inode spans
 * across one or two disk sectors.
 *
 * Inodes on disk do not contain certain memory-only members like inode_tag and
 * i_open_cnts, so these fields are cleared before writing. If the inode data
 * spans two sectors, both are read into 'io_buf', modified, and then written
 * back. For inodes fitting within a single sector, only that sector is
 * processed.
 *
 * Note: The function assumes that 'io_buf' is large enough to hold two disk
 * sectors.
 */
void inode_sync(struct partition *part, struct inode *inode, void *io_buf) {
  uint32_t inode_NO = inode->i_NO;
  struct inode_position inode_pos;
  inode_locate(part, inode_NO, &inode_pos);
  ASSERT(inode_pos.sector_LBA <= (part->start_LBA + part->sector_cnt));

  struct inode pure_inode;
  memcpy(&pure_inode, inode, sizeof(struct inode));
  /* strip out unused part of inode for the disk  */
  pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;
  pure_inode.i_open_cnt = 0;
  pure_inode.write_deny = false;

  char *inode_buf = (char *)io_buf;
  if (inode_pos.is_inode_cross_sectors) {
    /* Read the 2 sectors occupied by the inode  */
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 2);
    /* Splice the data, that is, write the new inode data to the location of the
     * inode on the two disks.  */
    memcpy((inode_buf + inode_pos.offset_in_sector), &pure_inode,
           sizeof(struct inode));
    ide_write(part->which_disk, inode_pos.sector_LBA, inode_buf, 2);
  } else {
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 1);
    memcpy((inode_buf + inode_pos.offset_in_sector), &pure_inode,
           sizeof(struct inode));
    ide_write(part->which_disk, inode_pos.sector_LBA, inode_buf, 1);
  }
}

/**
 * inode_open() - Open an inode by its number.
 * @part: Pointer to the partition where the inode resides.
 * @inode_no: The number of the inode to open.
 *
 * This function opens an inode with the given inode number from the specified
 * partition. It first searches the partition's open inode list and returns the
 * inode if found. If the inode is not in the open list, the function reads it
 * from the disk, adds it to the open list, and returns it. This function
 * handles the case where an inode spans two sectors. The function ensures that
 * the inode memory is allocated from the kernel memory pool
 */
struct inode *inode_open(struct partition *part, uint32_t inode_NO) {
  struct list_elem *inode_iter = part->open_inodes.head.next;
  struct inode *inode_found;

  /****************************************************************************/
  /* find target inode in part's open_inodes, which is the inode cache of the
  already opened inode */
  /****************************************************************************/
  while (inode_iter != &part->open_inodes.tail) {
    inode_found = elem2entry(struct inode, inode_tag, inode_iter);
    if (inode_found->i_NO == inode_NO) {
      inode_found->i_open_cnt++;
      return inode_found;
    }
    inode_iter = inode_iter->next;
  }

  /****************************************************************************/
  /* the target inode is not in open_inodes of part, read it from disk to
   * memory */
  /****************************************************************************/
  struct inode_position inode_pos;
  inode_locate(part, inode_NO, &inode_pos);

  /* set pg_dir to NULL temporarily so that sys_malloc mistakenly think it is a
   * kernel thread, ensuring that the inode allocates memory from the kernel
   * heap space, thereby realizing the sharing of inodes between tasks. */
  struct task_struct *cur = running_thread();
  uint32_t *cur_pgdir_backup = cur->pg_dir;
  cur->pg_dir = NULL;
  inode_found = (struct inode *)sys_malloc(sizeof(struct inode));
  cur->pg_dir = cur_pgdir_backup;

  char *inode_buf;
  if (inode_pos.is_inode_cross_sectors) {
    inode_buf = (char *)sys_malloc(512 * 2);
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 2);
  } else {
    inode_buf = (char *)sys_malloc(512);
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 1);
  }
  memcpy(inode_found, inode_buf + inode_pos.offset_in_sector,
         sizeof(struct inode));

  list_push(&part->open_inodes, &inode_found->inode_tag);
  inode_found->i_open_cnt = 1;
  sys_free(inode_buf);
  return inode_found;
}

/**
 * inode_close() - Close an inode or decrease its open count.
 * @inode: Pointer to the inode to be closed.
 *
 * This function decreases the open count of the given inode. If the open count
 * reaches zero, indicating no more processes are using this inode, it removes
 * the inode from the open inode list of its partition and frees the memory. The
 * function ensures that the inode memory is freed from the kernel memory pool.
 */
void inode_close(struct inode *inode) {
  enum intr_status old_status = intr_disable();
  if (--inode->i_open_cnt == 0) {
    list_remove(&inode->inode_tag);
    struct task_struct *cur = running_thread();
    uint32_t *cur_pgdir_backup = cur->pg_dir;
    cur->pg_dir = NULL;
    sys_free(inode);
    cur->pg_dir = cur_pgdir_backup;
  }
  intr_set_status(old_status);
}

/**
 * inode_init() - Initialize a new inode.
 * @inode_no: The number of the new inode.
 * @new_inode: Pointer to the inode structure to be initialized.
 *
 * This function initializes a new inode with the specified inode number. It
 * sets up the inode size, open counts, write permission, and initializes all
 * block indexes in the inode's sector array to 0. This includes setting up the
 * direct and indirect block pointers.
 */
void inode_init(uint32_t inode_NO, struct inode *new_inode) {
  new_inode->i_NO = inode_NO;
  new_inode->i_open_cnt = 0;
  new_inode->i_size = 0;
  new_inode->write_deny = false;

  uint8_t sector_idx = 0;
  while (sector_idx < 13) {
    new_inode->i_blocks[sector_idx] = 0;
    sector_idx++;
  }
}

/**
 * inode_delete() - Clears an inode on a partition.
 * @part: Pointer to the partition where the inode resides.
 * @inode_no: The inode number to be deleted.
 * @io_buf: Buffer used for disk I/O operations.
 *
 * This function erases an inode's data from the specified partition 'part'.
 * It locates the inode on the disk using its number 'inode_no' and sets its
 * content to zeros. This operation affects the disk sectors where the inode
 * is stored. Depending on whether the inode spans across one or two disk
 * sectors, it reads the necessary sectors into 'io_buf', zeros out the inode's
 * part, and writes it back to the disk.
 *
 * Note: The function assumes that 'io_buf' is large enough to hold at least one
 * disk sector.
 */
void inode_delete(struct partition *part, uint32_t inode_NO, void *io_buf) {
  ASSERT(inode_NO < 4096);
  struct inode_position inode_pos;
  inode_locate(part, inode_NO, &inode_pos);
  ASSERT(inode_pos.sector_LBA <= (part->start_LBA + part->sector_cnt));

  char *inode_buf = (char *)io_buf;
  /******** erase inode ********/
  if (inode_pos.is_inode_cross_sectors) {
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 2);
    memset((inode_buf + inode_pos.offset_in_sector), 0, sizeof(struct inode));
    ide_write(part->which_disk, inode_pos.sector_LBA, inode_buf, 2);
  } else {
    ide_read(part->which_disk, inode_pos.sector_LBA, inode_buf, 1);
    memset((inode_buf + inode_pos.offset_in_sector), 0, sizeof(struct inode));
    ide_write(part->which_disk, inode_pos.sector_LBA, inode_buf, 1);
  }
}

/**
 * inode_release() - Releases an inode and its associated data blocks.
 * @part: Pointer to the partition where the inode resides.
 * @inode_no: The inode number to be released.
 *
 * This function releases an inode and its associated data blocks from the
 * partition 'part'. It first opens the inode corresponding to 'inode_no',
 * then proceeds to release all data blocks used by the inode, including
 * direct and indirect blocks. The function also clears the corresponding bits
 * in the block bitmap and inode bitmap of the partition to indicate that these
 * blocks and the inode are now free. Additionally, it updates the bitmaps on
 * the disk to keep the filesystem state consistent.
 *
 * Note: Inode deletion from the inode table in the disk (zeroing its content)
 * is performed for debugging purposes and is not necessary for inode release.
 * The control of inode allocation is managed by the inode bitmap.
 */
void inode_release(struct partition *part, uint32_t inode_NO) {
  struct inode *inode_to_del = inode_open(part, inode_NO);
  ASSERT(inode_to_del->i_NO == inode_NO);

  uint8_t block_idx = 0;

  /* block_cnt record the number of blocks to be reclaimed  */
  uint8_t block_cnt = 12;

  uint32_t block_bitmap_idx;

  /* all_blocks_addr stores all the addresses of all blocks for 'part'
   */
  uint32_t all_blocks_addr[140] = {0};

  /******** gather addresses of all blocks ********/
  while (block_idx < 12) {
    all_blocks_addr[block_idx] = inode_to_del->i_blocks[block_idx];
    block_idx++;
  }

  if (inode_to_del->i_blocks[12] != 0) {
    ide_read(part->which_disk, inode_to_del->i_blocks[12], all_blocks_addr + 12,
             1);
    block_cnt += 128;

    /* Reclaim sectors occupied by the first-level indirect block table  */
    block_bitmap_idx = inode_to_del->i_blocks[12] - part->sup_b->data_start_LBA;
    ASSERT(block_bitmap_idx > 0);
    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }

  /******** now! erase the blocks, which means reclaim the corresponding free
   * block bit in inode_bitmap ********/
  block_idx = 0;
  while (block_idx < block_cnt) {
    if (all_blocks_addr[block_idx] != 0) {
      block_bitmap_idx = 0;
      block_bitmap_idx =
          all_blocks_addr[block_idx] - part->sup_b->data_start_LBA;
      ASSERT(block_bitmap_idx > 0);
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }
    block_idx++;
  }
  /* reclaim the corresponding inode bit in inode_bitmap   */
  bitmap_set(&part->inode_bitmap, inode_NO, 0);
  bitmap_sync(cur_part, inode_NO, INODE_BITMAP);

  void *io_buf = sys_malloc(1024);
  inode_delete(part, inode_NO, io_buf);
  sys_free(io_buf);
  inode_close(inode_to_del);
}
