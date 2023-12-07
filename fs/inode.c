/*
 * Author: Xun Morris
 * Time: 2023-12-05
 */
#include "inode.h"
#include "debug.h"
#include "ide.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

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
 * inode_sync() - Write an inode to a partition.
 * @part: Partition to which the inode will be written.
 * @inode: Pointer to the inode to be written.
 * @io_buf: Buffer used for disk I/O operations.
 *
 * This function synchronizes an inode from memory to disk. It first locates the
 * inode on disk, then prepares a clean version of the inode, stripping out
 * fields that are only relevant in memory (such as inode_tag and i_open_cnts).
 * The function handles writing the inode to the disk, dealing with cases where
 * the inode spans across one or two sectors.
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
