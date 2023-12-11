/*
 * Author: Zhang Xun
 * Time: 2023-12-02
 */
#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"

/*
 * Define selector attribute
 *
 * TI and RPL
 */
#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

/* Table Indicator  */
#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_KERNEL_CODE ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_KERNEL_DATA ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_KERNEL_STACK SELECTOR_KERNEL_DATA
/* GS represents video memory */
#define SELECTOR_KERNEL_GS ((3 << 3) + (TI_GDT << 2) + RPL0)
/* The fourth descriptor is TSS */
#define SELECTOR_U_CODE ((5 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA ((6 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA

/*
 * Define IDT attribute, which is bit 8 ~15, 8 bits in total.
 *
 * As to The type field of Descriptor
 * In 32-bits mode:
 * is D_1_1_0, where D is 1 , so 01110 =0xE
 * In 16 bits mode:
 * D is 0, so 00110 = 0x6
 * see P.305 for details
 */
#define IDT_DESC_P 1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3

#define IDT_DESC_32_TYPE 0xE
#define IDT_DESC_16_TYPE 0x6

#define IDT_DESC_ATTR_DPL0                                                     \
  ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3                                                     \
  ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

/*
 * macro in C program
 */
#define NULL ((void *)0)
#define bool int
#define true 1
#define false 0

/*
 * GDT attribute
 */
#define DESC_G_4K 1
#define DESC_D_32 1
#define DESC_L 0
#define DESC_AVL 0
#define DESC_P 1
#define DESC_DPL_0 0
#define DESC_DPL_1 1
#define DESC_DPL_2 2
#define DESC_DPL_3 3

#define DESC_S_CODE 1
#define DESC_S_DATA DESC_S_CODE
#define DESC_S_SYS 0

#define DESC_TYPE_CODE 8
#define DESC_TYPE_DATA 2
/* 1001 (10B1, B=0)*/
#define DESC_TYPE_TSS 9

#define GDT_ATTR_HIGH                                                          \
  ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))
#define GDT_CODE_ATTR_LOW_WITH_DPL3                                            \
  ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + (DESC_TYPE_CODE))
#define GDT_DATA_ATTR_LOW_WITH_DPL3                                            \
  ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + (DESC_TYPE_DATA))

/*
 * TSS attribute
 */
#define TSS_DESC_0 0
#define TSS_ATTR_HIGH                                                          \
  ((DESC_G_4K << 7) + (TSS_DESC_0 << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0)
#define TSS_ARRT_LOW                                                           \
  ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + (DESC_TYPE_TSS))

#define SELECTOR_TSS ((4 << 3) + (TI_GDT << 2) + RPL0)

struct gdt_desc {
  uint16_t limit_low_word;
  uint16_t base_low_word;
  uint8_t base_mid_byte;
  uint8_t attr_low_byte;
  uint8_t limit_high_attr_high;
  uint8_t base_high_byte;
};

#define EFLAGS_MBS (1 << 1)
#define EFLAGS_IF_0 0
#define EFLAGS_IF_1 (1 << 9)

#define EFLAGS_IOPL_3 (3 << 12)
#define EFLAGS_IOPL_0 (0 << 12)

#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))
#define PAGE_SIZE 4096

#define UNUSED __attribute__((unused))

#endif
