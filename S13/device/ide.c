/*
 * Author: Xun Morris
 * Time: 2023-12-01
 */
#include "ide.h"
#include "debug.h"
#include "global.h"
#include "io.h"
#include "stdint.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "sync.h"
#include "timer.h"
#include <stdint.h>
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
#define reg_alte_status(channel) (channel->port_base + 0x206)
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

static void select_disk(struct disk *hd) {
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
  if (hd->dev_NO == 1)
    reg_device |= BIT_DEV_SLAVE;
  outb(reg_device(hd->which_channel), reg_device);
}

/* write LBA and sec_cnt info to hard disk controller  */
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

/* Issue command cmd to channel */
static void cmd_out(struct ide_channel *channel, uint8_t cmd) {
  channel->expecting_intr = true;
  outb(reg_cmd(channel), cmd);
}

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

static bool busy_wait(struct disk *hd) {
  struct ide_channel *channel = hd->which_channel;
  /* wait 30 seconds  */
  uint16_t time_limit = 30 * 1000;
  while (time_limit -= 10 >= 0) {
    if (!(inb(reg_status(channel) & BIT_STAT_BUSY))) {
      /* disk is not busy  */
      return (inb(reg_status(channel) & BIT_STAT_DREQ));
    } else {
      mtime_sleep(10);
    }
  }
  return false;
}

/* read sector_cnt sectors from sector address LBA on hard-disk hd to buf  */
void ide_read(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt) {
  ASSERT(LBA <= MAX_LBA && sector_cnt > 0);
  lock_acquire(&hd->which_channel->_lock);
  select_disk(hd);

  /* Number of sectors to be read  */
  uint32_t sector_operate;
  /* Number of sectors that have been read */
  uint32_t sector_done;
  while (sector_done < sector_cnt) {
    if ((sector_done + 256) <= sector_cnt) {
      sector_operate = 256
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
      sprintf(error_msg, "%s read sector %d failed!!!!!!", hd->name, LBA);
      PANIC(error_msg);
    }

    read_from_sector(hd, (void *)((uint32_t)buf + sector_done * 512),
                     sector_operate);
    sector_done += sector_operate;
  }
  lock_release(&hd->which_channel->_lock);
}

void ide_write(struct disk *hd, uint32_t LBA, void *buf, uint32_t sector_cnt) {
  ASSERT(LBA <= MAX_LBA && sector_cnt > 0);
  lock_acquire(&hd->which_channel->_lock);
  select_disk(hd);

  /* Number of sectors to be read  */
  uint32_t sector_operate;
  /* Number of sectors that have been read */
  uint32_t sector_done;
  while (sector_done < sector_cnt) {
    if ((sector_done + 256) <= sector_cnt) {
      sector_operate = 256
    } else {
      sector_operate = sector_cnt - sector_done;
    }

    select_sector(hd, LBA + sector_done, sector_operate);
    cmd_out(hd->which_channel, CMD_WRITE_SECTOR);

    if (!busy_wait(hd)) {
      char error_msg[64];
      sprintf(error_msg, "%s write sector %d failed!!!!!!", hd->name, LBA);
      PANIC(error_msg);
    }

    write_to_sector(hd, (void *)((uint32_t)buf + sector_done * 512),
                    sector_operate);
    sema_down(&hd->which_channel->disk_done);
    sector_done += sector_operate;
  }
  lock_release(&hd->which_channel->_lock);
}

void intr_hd_handler(uint32_t _IRQ_NO) {
  ASSERT(_IRQ_NO == 0x2e || _IRQ_NO == 0x2f);
  uint8_t channel_NO = _IRQ_NO - 0x2e;
  struct ide_channel *channel = &channels[channel_NO];
  ASSERT(channel->IRQ_NO == _IRQ_NO);

  if (channel->expecting_intr) {
    channel->expecting_intr = false;
    sema_up(&channel->disk_done);

    inb(reg_status(channel));
  }
}

void ide_init() {
  printk("ide_init start\n");
  /* Get the number of hard drives from virtual address 0x475 (which
   * corresponds to physical address 0x475 *^_^*) */
  uint8_t hd_cnt = *((uint8_t *)0x475);
  ASSERT(hd_cnt > 0);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);

  struct ide_channel *channel;
  uint8_t channel_NO = 0;
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
    channel_NO++;
  }
  printk("ide_init done\n");
}
