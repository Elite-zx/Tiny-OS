/*
 * Author: Xun Morris
 * Time: 2023-12-01
 */
#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "bitmap.h"
#include "list.h"
#include "stdint.h"
#include "sync.h"

/**
 * struct partition - Represents a disk partition.
 * @start_lba: The starting sector of the partition.
 * @sector_cnt: The number of sectors in the partition.
 * @my_disk: Pointer to the disk containing this partition.
 * @part_tag: List element for queueing.
 * @name: Name of the partition.
 * @sb: Superblock of the partition.
 * @block_bitmap: Bitmap for block allocation.
 * @inode_bitmap: Bitmap for inode allocation.
 * @open_inodes: List of open inodes in the partition.
 *
 * This structure represents a single partition on a disk, containing
 * metadata about the partition and structures to manage its filesystem.
 */
struct partition {
  uint32_t start_LBA;
  uint32_t sector_cnt;
  struct disk *which_disk;
  struct list_elem part_tag;
  char name[8];
  struct super_block *sb;
  struct bitmap block_bitmap;
  struct bitmap inode_bitmap;
  struct list open_inodes;
};

/**
 * struct disk - Represents a physical hard disk.
 * @name: Name of the disk (e.g.'sda','sdb').
 * @which_channel: The IDE channel this disk is attached to.
 * @dev_no: Device number (0 for master, 1 for slave).
 * @prim_parts: Array of primary partitions on the disk (4 max).
 * @logic_parts: Array of logical partitions on the disk.
 *
 * This structure represents a physical hard disk and includes
 * data about its partitions and the IDE channel it is connected to.
 */
struct disk {
  char name[8];
  struct ide_channel *which_channel;
  uint8_t dev_NO;
  struct partition prim_parts[4];
  struct partition logic_parts[8];
};

/**
 * struct ide_channel - Represents an ATA channel.
 * @name: Name of the ATA channel.
 * @port_base: Base port number for the channel.
 * @irq_no: Interrupt number used by the channel.
 * @lock: Lock for synchronizing access to the channel.
 * @expecting_intr: Flag to indicate if an interrupt is expected.
 * @disk_done: Semaphore for blocking and waking up the driver.
 * @devices: Array of disks (2 max) connected to the channel.
 *
 * Represents an ATA channel to which disks are connected. It manages
 * access to these disks and synchronization between them and the interrupt
 * system. ide means "intergrated drive electronics"
 */
struct ide_channel {
  char name[8];
  int16_t port_base;
  uint8_t IRQ_NO;
  struct lock _lock;
  bool expecting_intr;
  struct semaphore disk_done;
  struct disk devices[2];
};

void ide_init();
void ide_write(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt);
void ide_read(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt);
#endif
