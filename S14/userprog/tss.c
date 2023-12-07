/*
 * Author: Xun Morris
 * Time: 2023-11-25
 */
#include "tss.h"
#include "global.h"
#include "print.h"
#include "stdint.h"
#include "string.h"
#include "thread.h"

struct tss {
  uint32_t backlink;
  uint32_t *esp0;
  uint32_t ss0;
  uint32_t *esp1;
  uint32_t ss1;
  uint32_t *esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t (*eip)(void);
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;
  uint32_t io_base;
};
static struct tss tss;

/**
 * update_tss_esp() - Update the ESP0 field in the TSS.
 * @pthread: Pointer to the task_struct whose 0-level stack will be used for
 * ESP0.
 *
 * This function updates the Task State Segment (TSS) ESP0 field to point to the
 * top of the 0-level stack of the given task structure (pthread). This is
 * essential for proper context switching, especially when transitioning from
 * user mode to kernel mode.
 */
void update_tss_esp(struct task_struct *pthread) {
  tss.esp0 = (uint32_t *)((uint32_t)pthread + PAGE_SIZE);
}

/**
 * make_gdt_desc() - Create a descriptor for the Global Descriptor Table (GDT).
 * @desc_addr: Base address of the descriptor.
 * @limit: The limit of the segment.
 * @attr_low: Lower 8 bits of the segment attribute.
 * @attr_high: Higher 8 bits of the segment attribute.
 *
 * Returns a structure representing a GDT descriptor. The descriptor includes
 * the segment's base address, limit, and attributes. This function is used for
 * setting up segments in the GDT, such as code, data, and TSS segments.
 */
static struct gdt_desc make_gdt_desc(uint32_t *desc_addr, uint32_t limit,
                                     uint8_t attr_low, uint8_t attr_high) {
  uint32_t desc_base = (uint32_t)desc_addr;
  struct gdt_desc desc;
  desc.limit_low_word = limit & 0x0000ffff;
  desc.limit_high_attr_high =
      (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
  desc.base_low_word = desc_base & 0x0000ffff;
  desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
  desc.base_high_byte = desc_base >> 24;
  desc.attr_low_byte = (uint8_t)attr_low;
  return desc;
}

/**
 * tss_init() - Initialize the Task State Segment (TSS) and load the GDT.
 *
 * This function initializes the TSS with default values, including setting up
 * the stack segment and I/O bitmap. It also creates descriptors in the GDT for
 * the TSS and for DPL 3 code and data segments. Finally, it loads the new GDT
 * and sets the Task Register to use the new TSS. This function is crucial for
 * setting up a working environment for task switching and privilege level
 * changes.
 */
void tss_init() {
  put_str("tss_init start\n");
  uint32_t tss_size = sizeof(tss);
  memset(&tss, 0, tss_size);
  tss.ss0 = SELECTOR_KERNEL_STACK;
  tss.io_base = tss_size;

  *((struct gdt_desc *)0xc0000920) = make_gdt_desc(
      (uint32_t *)&tss, tss_size - 1, TSS_ARRT_LOW, TSS_ATTR_HIGH);
  /* the size of code/data segment is  4GB (2^20 * 2^12 = 2^32) */
  *((struct gdt_desc *)0xc0000928) = make_gdt_desc(
      (uint32_t *)0, 0xfffff, GDT_CODE_ATTR_LOW_WITH_DPL3, GDT_ATTR_HIGH);
  *((struct gdt_desc *)0xc0000930) = make_gdt_desc(
      (uint32_t *)0, 0xfffff, GDT_DATA_ATTR_LOW_WITH_DPL3, GDT_ATTR_HIGH);

  uint64_t lgdt_operand =
      ((8 * 7 - 1) | ((uint64_t)(uint32_t)0xc0000900 << 16));
  asm volatile("lgdt %0" ::"m"(lgdt_operand));
  asm volatile("ltr %w0" ::"r"(SELECTOR_TSS));
  put_str("tss_init done\n");
}
