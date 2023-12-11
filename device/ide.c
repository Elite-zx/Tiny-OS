/*
 * Author: Zhang Xun
 * Time: 2023-12-01
 */
#include "ide.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "io.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "string.h"
#include "sync.h"
#include "timer.h"

/* port number decided by channel number  */
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sector_cnt(channel) (channel->port_base + 2)
#define reg_LBA_l(channel) (channel->port_base + 3)
#define reg_LBA_m(channel) (channel->port_base + 4)
#define reg_LBA_h(channel) (channel->port_base + 5)
#define reg_device(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_altne_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) (reg_alte_status(channel))

/**
 * special bit in status register
 */
/* bit 7, disk is busy  */
#define BIT_STAT_BUSY 0x80
/* bit 6, disk is ready, which means disk is ready to execute the command  */
#define BIT_STAT_DRDY 0x40
/* bit 3, disk is data requesting, which means data in disk is ready */
#define BIT_STAT_DREQ 0x08
/**
 * special bit in device register  *
 */
#define BIT_DEV_MBS 0xa0
/* bit 6, enable LBA mode  */
#define BIT_DEV_LBA 0x40
/* bit 4, chose disk (master disk or *slave disk)  */
#define BIT_DEV_SLAVE 0x10

/**
 * operation command on disk
 */
#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTOR 0x20
#define CMD_WRITE_SECTOR 0x30

/* The maximum number of sectors that can be read and written on an 80MB disk */
#define MAX_LBA ((80 * 1024 * 1024 / 512) - 1)

uint8_t channel_cnt;
/* There are up to 2 IDE slots on a mainboard, which means up to 2 channels,
 * which means up to 4 disks  */
struct ide_channel channels[2];

int32_t ext_LBA_benchmark = 0;
uint8_t primary_disk_NO = 0, logical_disk_NO = 0;
struct list partition_list;

/**
 * struct partition_table_entry - Structure for partition table entry.
 * @bootable: Whether the partition is bootable.
 * @start_head: Starting head number.
 * @start_sec: Starting sector number.
 * @start_CHS: Starting cylinder number.
 * @fs_type: File system type.
 * @end_head: Ending head number.
 * @end_sector: Ending sector number.
 * @end_CHS: Ending cylinder number.
 * @start_offset_LBA: LBA address of the starting offset sector of the
 * partition.
 * @sector_cnt: Number of sectors in the partition (CAPACITY).
 *
 * This structure represents a partition table entry in a Master Boot Record
 * (MBR). It includes details about the partition layout and type.
 * __attribute__((packed)) here is ask gcc to make sure that this structure's
 * size is 16 bytes
 */
struct partition_table_entry {
  uint8_t bootable;
  uint8_t start_head;
  uint8_t start_sector;
  uint8_t start_CHS;
  uint8_t fs_type;
  uint8_t end_head;
  uint8_t end_sector;
  uint8_t end_CHS;

  uint32_t start_offset_LBA;
  uint32_t sector_cnt;
} __attribute__((packed));

/**
 * struct boot_sector - Structure for MBR/EBR
 * @other: placeholder, which represents boot code
 * @partition_table: the partition table has 4 entries, a total of 64 bytes
 * @signature:magic number --- 0x55,0xaa, which is the mark of MBR
 *
 * __attribute__((packed)) here is ask gcc to  make sure that this structure's
 * size is 512 bytes
 */
struct boot_sector {
  uint8_t other[446];
  struct partition_table_entry partition_table[4];
  uint16_t signature;
} __attribute__((packed));

/**
 * swap_pairs_bytes() - Swap adjacent bytes in a string.
 * @dst: Pointer to the source string.
 * @buf: Pointer to the buffer where the result will be stored.
 * @len: The number of bytes in the source string to process.
 *
 * This function takes a string from 'dst', swaps each pair of adjacent bytes,
 * and stores the result in 'buf'. It is designed to handle strings where the
 * characters are in reversed byte order. The resulting string in 'buf' is
 * null-terminated.
 */
static void swap_pairs_bytes(const char *dst, char *buf, uint32_t len) {
  uint8_t idx;
  for (idx = 0; idx < len; idx += 2) {
    buf[idx + 1] = *dst++;
    buf[idx] = *dst++;
  }
  buf[idx] = '\0';
}

/**
 * select_disk() - Select the disk for read/write operations.
 * @hd: Pointer to the disk structure to be selected.
 *
 * This function selects the disk for subsequent read/write operations.
 * It sets the device register based on whether the disk is a master or slave.
 *
 * Context: Should not be called in an interrupt context where outb() may sleep.
 */
static void select_disk(struct disk *hd) {
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
  if (hd->dev_NO == 1)
    reg_device |= BIT_DEV_SLAVE;
  outb(reg_device(hd->which_channel), reg_device);
}

/**
 * select_sector() - Write start sector address and number of sectors to
 * read/write.
 * @hd: Pointer to the disk structure.
 * @LBA: Logical Block Addressing start sector.
 * @sector_cnt: Number of sectors to read/write.
 *
 * Writes the number of sectors to read/write to the disk controller,
 * and the starting LBA (Logical Block Address). It deals with the 28-bit LBA
 * addressing.
 *
 * Context: Should not be called in an interrupt context as it accesses I/O
 * ports. Return: None.
 */
static void select_sector(struct disk *hd, uint32_t LBA, uint8_t sector_cnt) {
  ASSERT(LBA <= MAX_LBA);

  /* set sector count  */
  struct ide_channel *channel = hd->which_channel;
  outb(reg_sector_cnt(channel), sector_cnt);

  /* set LBA */
  outb(reg_LBA_l(channel), LBA);
  outb(reg_LBA_m(channel), LBA >> 8);
  outb(reg_LBA_h(channel), LBA >> 16);
  outb(reg_device(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                                (hd->dev_NO == 1 ? BIT_DEV_SLAVE : 0) |
                                LBA >> 24);
}

/**
 * cmd_out() - Send command to the IDE channel.
 * @channel: Pointer to the IDE channel structure.
 * @cmd: Command byte to be sent.
 *
 * This function sends a command to the specified IDE channel and marks
 * that an interrupt is expected in response to this command.
 *
 * Context: Assumes the caller holds appropriate locks if necessary.
 *          Should not be called in interrupt context.
 * Return: None.
 */
static void cmd_out(struct ide_channel *channel, uint8_t cmd) {
  channel->expecting_intr = true;
  outb(reg_cmd(channel), cmd);
}

/**
 * read_from_sector() - Read data from disk into buffer.
 * @hd: Pointer to the disk structure.
 * @buf: Buffer to store read data.
 * @sector_cnt: Number of sectors to read.
 *
 * Reads 'sector_cnt' sectors from the disk into 'buf'. Handles edge case when
 * 'sector_cnt' is zero (reads 256 sectors).
 *
 * Context: Should not be called in interrupt context.
 * Return: None.
 */
static void read_from_sector(struct disk *hd, void *buf, uint8_t sector_cnt) {
  uint32_t size_in_byte;
  if (sector_cnt == 0) {
    /* sector_cnt is 256  */
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sector_cnt * 512;
  }
  /* 16bits at once*/
  insw(reg_data(hd->which_channel), buf, size_in_byte / 2);
}

/**
 * write_to_sector() - Write data from buffer to disk.
 * @hd: Pointer to the disk structure.
 * @buf: Buffer containing data to write.
 * @sector_cnt: Number of sectors to write.
 *
 * Writes 'sector_cnt' sectors from 'buf' to the disk. Handles edge case when
 * 'sector_cnt' is zero (writes 256 sectors).
 *
 * Context: Should not be called in interrupt context.
 * Return: None.
 */
static void write_to_sector(struct disk *hd, void *buf, uint8_t sector_cnt) {
  uint32_t size_in_byte;
  if (sector_cnt == 0) {
    /* sector_cnt is 256  */
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sector_cnt * 512;
  }
  outsw(reg_data(hd->which_channel), buf, size_in_byte / 2);
}

/**
 * busy_wait() - Wait for the disk to become ready.
 * @hd: Pointer to the disk structure.
 *
 * Waits for up to 30 seconds for the disk to become ready for operations.
 * Checks the BUSY status bit in the status register.
 *
 * Context: Can be called in contexts where sleeping is permissible.
 * Return: True if the disk is ready, False otherwise.
 */
static bool busy_wait(struct disk *hd) {
  struct ide_channel *channel = hd->which_channel;
  /* wait 30 seconds  */
  uint16_t time_limit = 30 * 1000;

  while (time_limit -= 10 >= 0) {
    if (!(inb(reg_status(channel)) & BIT_STAT_BUSY)) {
      /* disk is not busy  */
      return (inb(reg_status(channel)) & BIT_STAT_DREQ);
    } else {
      /* disk is working now, yield (which means schedule to other
       * processes/threads) */
      mtime_sleep(10);
    }
  }
  return false;
}

/**
 * ide_read() - Read sectors from disk into buffer.
 * @hd: Pointer to the disk structure.
 * @LBA: Logical Block Addressing start sector.
 * @buf: Buffer to store read data.
 * @sector_cnt: Number of sectors to read.
 *
 * Reads 'sector_cnt' sectors starting from 'LBA' into 'buf'. Locks the channel
 * during operation and handles multiple sector reads.
 *
 * Context: Should be called in a context where sleeping is permissible.
 * Return: None.
 */
void ide_read(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt) {
  ASSERT(LBA <= MAX_LBA && sector_cnt > 0);
  lock_acquire(&hd->which_channel->_lock);
  select_disk(hd);

  /* Number of sectors to be read  */
  uint32_t sector_operate;
  /* Number of sectors that have been read */
  uint32_t sector_done = 0;
  while (sector_done < sector_cnt) {
    if ((sector_done + 256) <= sector_cnt) {
      sector_operate = 256;
    } else {
      sector_operate = sector_cnt - sector_done;
    }

    select_sector(hd, LBA + sector_done, sector_operate);
    /*Let the hard drive start working which is reading data  */
    cmd_out(hd->which_channel, CMD_READ_SECTOR);

    /* semaphore disk_done's initial value is 0, so sema_down means block
     * itself, that is, hard disk driver */
    sema_down(&hd->which_channel->disk_done);

    /* hard disk driver is Woken up by hard disk interrupt handler */

    if (!busy_wait(hd)) {
      char error_msg[64];
      sprintf(error_msg, "%s read sector %d failed!!!!!!\n", hd->name, LBA);
      PANIC(error_msg);
    }

    read_from_sector(hd, (void *)((uint32_t)buf + sector_done * 512),
                     sector_operate);
    sector_done += sector_operate;
  }
  lock_release(&hd->which_channel->_lock);
}

/**
 * ide_write() - Write sectors from buffer to disk.
 * @hd: Pointer to the disk structure.
 * @LBA: Logical Block Addressing start sector.
 * @buf: Buffer containing data to write.
 * @sector_cnt: Number of sectors to write.
 *
 * Writes 'sector_cnt' sectors from 'buf' to disk starting from 'LBA'. Locks the
 * channel during operation and handles multiple sector writes.
 *
 * Context: Should be called in a context where sleeping is permissible.
 * Return: None.
 */
void ide_write(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt) {
  ASSERT(LBA <= MAX_LBA && sector_cnt > 0);
  lock_acquire(&hd->which_channel->_lock);
  select_disk(hd);

  /* Number of sectors to be read  */
  uint32_t sector_operate;
  /* Number of sectors that have been read */
  uint32_t sector_done = 0;
  while (sector_done < sector_cnt) {
    if ((sector_done + 256) <= sector_cnt) {
      sector_operate = 256;
    } else {
      sector_operate = sector_cnt - sector_done;
    }

    select_sector(hd, LBA + sector_done, sector_operate);
    cmd_out(hd->which_channel, CMD_WRITE_SECTOR);

    /* Check whether the disk is ready for transfer */
    if (!busy_wait(hd)) {
      char error_msg[64];
      sprintf(error_msg, "%s write sector %d failed!!!!!!\n", hd->name, LBA);
      PANIC(error_msg);
    }

    write_to_sector(hd, (void *)((uint32_t)buf + sector_done * 512),
                    sector_operate);
    sema_down(&hd->which_channel->disk_done);
    sector_done += sector_operate;
  }
  lock_release(&hd->which_channel->_lock);
}

/**
 * intr_hd_handler() - Hard disk interrupt handler.
 * @irq_no: The IRQ number that triggered the interrupt.
 *
 * This is the interrupt handler for hard disk operations. It verifies the IRQ
 * number, retrieves the corresponding IDE channel, and processes the interrupt
 * if it is expected. This handler acknowledges the interrupt to the disk
 * controller and signals completion of disk operations.
 */
void intr_hd_handler(uint8_t _IRQ_NO) {
  ASSERT(_IRQ_NO == 0x2e || _IRQ_NO == 0x2f);
  uint8_t channel_NO = _IRQ_NO - 0x2e;
  struct ide_channel *channel = &channels[channel_NO];
  ASSERT(channel->IRQ_NO == _IRQ_NO);

  if (channel->expecting_intr) {
    channel->expecting_intr = false;
    sema_up(&channel->disk_done);

    /* Inform the disk controller that the interrupt has completed, so that disk
     * controller would not sent interrupt signal repeatedly
     */
    inb(reg_status(channel));
  }
}

/**
 * identify_disk() - Retrieve and display disk identification information.
 * @hd: Pointer to the disk structure.
 *
 * This function sends an IDENTIFY command to the specified disk and waits for
 * the operation to complete. It reads the disk identification information,
 * processes it, and prints out the disk's serial number, model, total number of
 * sectors, and capacity. The function utilizes swap_pairs_bytes to correct the
 * byte order of the serial number and model information.
 */
static void identify_disk(struct disk *hd) {
  char id_info[512];
  select_disk(hd);
  cmd_out(hd->which_channel, CMD_IDENTIFY);

  /* disk start working, I (the hard driver) gonna to sleep -_- ᶻ𝗓 , CPU will
   * execute other processes/threads or thread idle */
  sema_down(&hd->which_channel->disk_done);

  /* hard disk driver is Woken up by hard disk interrupt handler */
  if (!busy_wait(hd)) {
    char error_msg[64];
    sprintf(error_msg, "%s identify failed!!!!!!\n", hd->name);
    PANIC(error_msg);
  }
  read_from_sector(hd, id_info, 1);

  /* get serial number, model number and available sector count of disk */
  char buf[64];
  uint8_t serial_num_start = 10 * 2, serial_num_len = 20;
  uint8_t model_start = 27 * 2, model_len = 40;
  uint8_t sector_cnt_start = 60 * 2;
  swap_pairs_bytes(&id_info[serial_num_start], buf, serial_num_len);
  printk(" disk %s info:\n      Serial-Number: %s\n", hd->name, buf);
  memset(buf, 0, sizeof(buf));
  swap_pairs_bytes(&id_info[model_start], buf, model_len);
  printk("      Model: %s\n", buf);
  uint32_t sectors = *((uint32_t *)&id_info[sector_cnt_start]);
  printk("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

/**
 * partition_scan() - Scan all partitions in a sector of a disk.
 * @hd: Pointer to the disk structure to be scanned.
 * @ext_lba: The sector address where the scan starts.
 *
 * This function scans for partition entries in a given sector of the specified
 * disk. It handles both primary and extended partitions. For extended
 * partitions, it recursively scans their entries. The function updates global
 * partition data structures(prim_parts and logic_parts in hd) with the found
 * partition information.
 */
static void partition_scan(struct disk *hd, uint32_t _LBA) {
  /* Do not use stack space because of recursion, use heap space instead  */
  struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
  ide_read(hd, _LBA, bs, 1);
  uint8_t part_idx = 0;
  struct partition_table_entry *p = bs->partition_table;

  while (part_idx++ < 4) {
    if (p->fs_type == 0x5) {
      /* handle extended partition  */
      if (ext_LBA_benchmark != 0) {
        /* sub-extended partition  */

        /* scan DPT in sub-extended partition (DPT in EBR)  */
        partition_scan(hd, p->start_offset_LBA + ext_LBA_benchmark);
      } else {
        /* main extended partition, in MBR DPT */

        /* The benchmark LBA of the sub-extended partition is the starting LBA
         * of the extended partition itself  */
        ext_LBA_benchmark = p->start_offset_LBA;

        /* This recursive call is to enter the if branch above to scan
         * sub-extended partition  */
        partition_scan(hd, p->start_offset_LBA);
      }
    } else if (p->fs_type != 0) {
      /* is valid partition (not empty)   */
      if (_LBA == 0) {
        /* handle main partition, I am scanning MBR now 0w0 */

        /* no offset for main partition  */
        hd->prim_parts[primary_disk_NO].start_LBA = p->start_offset_LBA;

        hd->prim_parts[primary_disk_NO].sector_cnt = p->sector_cnt;
        hd->prim_parts[primary_disk_NO].which_disk = hd;

        list_append(&partition_list, &hd->prim_parts[primary_disk_NO].part_tag);
        sprintf(hd->prim_parts[primary_disk_NO].name, "%s%d", hd->name,
                primary_disk_NO + 1);
        primary_disk_NO++;
        /* only 4 (0~3) entries in DPT  */
        ASSERT(primary_disk_NO < 4);
      } else {
        /* logical partition in sub-extended partition */

        /* The benchmark LBA of the logical partition is the starting LBA
         * of the sub-extended partition */
        hd->logic_parts[logical_disk_NO].start_LBA = _LBA + p->start_offset_LBA;

        hd->logic_parts[logical_disk_NO].sector_cnt = p->sector_cnt;
        hd->logic_parts[logical_disk_NO].which_disk = hd;
        list_append(&partition_list,
                    &hd->logic_parts[logical_disk_NO].part_tag);
        sprintf(hd->logic_parts[logical_disk_NO].name, "%s%d", hd->name,
                logical_disk_NO + 5);
        logical_disk_NO++;
        if (logical_disk_NO >= 8)
          return;
      }
    }
    /* next entry in DPT  */
    p++;
  }
  sys_free(bs);
}

/**
 * print_partition_info() - Print information about a partition.
 * @pelem: List element representing a partition.
 * @arg: Unused argument, present for compatibility with list traversal
 * function.
 *
 * This is a helper function used with list_traversal to print detailed
 * information about each partition. It extracts the partition structure from
 * the list element and prints its name, start LBA, and sector count. Always
 * returns false to continue list traversal.
 */
static bool print_partition_info(struct list_elem *pelem, int arg UNUSED) {
  /* get partition itself from its member  */
  struct partition *part = elem2entry(struct partition, part_tag, pelem);
  printk("   %s start_LBA:0x%x, sector_cnt:0x%x\n", part->name, part->start_LBA,
         part->sector_cnt);
  return false;
}

/**
 * ide_init() - Initialize IDE hard disk controller.
 *
 * This function initializes the IDE channels and associated hard disks. It
 * retrieves the number of hard disks, initializes data structures for each IDE
 * channel and disk, and scans for partitions on each disk. The function
 * registers the hard disk interrupt handler and prints out all found partition
 * information.
 */
void ide_init() {
  printk("ide_init start\n");
  /* Get the number of hard drives from virtual address 0x475 (which
   * corresponds to physical address 0x475 *^_^*) , the value is 2 */
  uint8_t hd_cnt = *((uint8_t *)0x475);
  ASSERT(hd_cnt > 0);
  /* channel_cnt is 1, only primary channel is available*/
  list_init(&partition_list);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);

  struct ide_channel *channel;
  uint8_t channel_NO = 0;
  uint8_t dev_NO = 0;
  /* initialize channel_cnt (1 in fact) channels  in mainbaord  */
  while (channel_NO < channel_cnt) {
    channel = &channels[channel_NO];
    sprintf(channel->name, "ide%d", channel_NO);
    switch (channel_NO) {
    case 0:
      channel->port_base = 0x1f0;
      channel->IRQ_NO = 0x20 + 14;
      break;
    case 1:
      channel->port_base = 0x170;
      channel->IRQ_NO = 0x20 + 15;
      break;
    }
    channel->expecting_intr = false;
    lock_init(&channel->_lock);
    sema_init(&channel->disk_done, 0);

    register_handler(channel->IRQ_NO, intr_hd_handler);
    /* get partition info from two disk in each channel  */
    while (dev_NO < 2) {
      struct disk *hd = &channel->devices[dev_NO];
      hd->which_channel = channel;
      hd->dev_NO = dev_NO;
      sprintf(hd->name, "sd%c", 'a' + channel_NO * 2 + dev_NO);
      identify_disk(hd);
      /* do nothing to hd60M.img, where the OS kernel locate  */
      if (dev_NO != 0) {
        /* read DPT in MBR and EBR,   */
        partition_scan(hd, 0);
      }
      primary_disk_NO = 0;
      logical_disk_NO = 0;
      dev_NO++;
    }
    channel_NO++;
    dev_NO = 0;
  }

  printk("\n all partition info as follows:\n");
  list_traversal(&partition_list, print_partition_info, 0);
  printk("ide_init done\n");
}
