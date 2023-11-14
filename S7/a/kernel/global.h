/*
 * Author: Xun Morris
 * Time: 2023-11-13
 */

#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"

/*
 * Define  selector attribute
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
#define SELECTOR_KERNEL_GS ((3 << 3) + (TI_GDT << 2) + RPL0)

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
#endif
