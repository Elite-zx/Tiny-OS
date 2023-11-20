/*
 * Author: Xun Morris
 * Time: 2023-11-20
 */

#include "interrupt.h"
#include "global.h"
#include "io.h"
#include "print.h"

/* CTRL/DATA port of main 8259A chip.*/
#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
/* CTRL/DATA port of slave 8259A chip.*/
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1

/* Interrupt Descriptor Count.*/
#define IDT_DESC_COUNT 0x21

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAGS_VAR) asm volatile("pushfl;popl %0" : "=g"(EFLAGS_VAR))

/*
 * struct gate_desc - Interrupt Descriptor Table entry (i.e. gate) struct.
 * @func_offset_low_word: 0~15 bits of func offset.
 * @selector: selector for target code segment.
 * @dcount:  unused part.
 * @attribute: fields TYPE, S, DPL, P.
 * @func_offset_high_word: 16~31 bits of func offset.
 *
 * 8 bytes in total.
 */
struct gate_desc {
  /* low 32-bits.*/
  uint16_t func_offset_low_word;
  uint16_t selector;

  /* high 32-bits.*/
  uint8_t dcount;
  uint8_t attribute;
  uint16_t func_offset_high_word;
};

/* Interrupt Descriptor Table. */
static struct gate_desc idt[IDT_DESC_COUNT];

char *intr_name[IDT_DESC_COUNT];

/* Interrupt handler address table  */
intr_handler idt_table[IDT_DESC_COUNT];

extern intr_handler intr_entry_table[IDT_DESC_COUNT];

/*
 * pic_init - set main/slave 8259A chips to the correct value, such as setting
 * the starting value of the interrupt vector number.
 *
 * This function set registers ICW1~4 and OCW1~4.
 */
static void pic_init() {
  outb(PIC_M_CTRL, 0x11); /* main chip's ICW1 */
  /* main chip's ICW2: set the starting interrupt number to 0x20 */
  outb(PIC_M_DATA, 0x20);
  /* main chip's ICW3: set IR2 to link the  slave chip */
  outb(PIC_M_DATA, 0x04);
  outb(PIC_M_DATA, 0x01); /* main chip's ICW4: set No AEOI, send EOI manually */

  outb(PIC_S_CTRL, 0x11); /* slave chip's ICW1  */
  outb(PIC_S_DATA, 0x28); /* slave chip's ICW2: ... is 0x28 */
  outb(PIC_S_DATA, 0x02); /* slave chip's ICW3 */
  outb(PIC_S_DATA, 0x01); /* slave chip's ICW4 */

  outb(PIC_M_DATA, 0xfe); /* OCW1 (M/S)  --- Only enable clock interrupts. */
  outb(PIC_S_DATA, 0xff);

  put_str("  pic_init done\n");
}

/*
 *  make_idt_desc - construct interrupt descriptor
 *  @pt_gdesc: interrupt gate descriptor point
 *  @attr: descriptor attribute
 *  @function: address of interrupt handler
 *
 *  This function write attr and function into descriptor pointed by pt_gdesc.
 */
static void make_idt_desc(struct gate_desc *pt_gdesc, uint8_t attr,
                          intr_handler function) {
  pt_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
  pt_gdesc->selector = SELECTOR_KERNEL_CODE;
  pt_gdesc->dcount = 0;
  pt_gdesc->attribute = attr;
  pt_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/*
 * idt_desc_init - initialize interrupt descriptor table
 *
 * This function populate the interrupt descriptor entry for IDT by calling
 * function make_idt_desc.
 */
static void idt_desc_init() {
  int i;
  for (i = 0; i < IDT_DESC_COUNT; ++i) {
    make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }

  put_str("  idt_desc_init done\n");
}

/**
 * general_intr_handler - print the interrupt vector number
 * @vec_nr: vector number to be printed
 *
 * This function is default interrupt handler for all interrupts.
 */
static void general_intr_handler(uint8_t vec_nr) {
  if (vec_nr == 0x27 || vec_nr == 0x2f)
    return;

  set_cursor(0);
  int cursor_pos = 0;
  while (cursor_pos < 320) {
    put_char(' ');
    ++cursor_pos;
  }
  set_cursor(0);
  put_str("!!!!!!      exception message begin      !!!!!!");
  set_cursor(88);
  put_str(intr_name[vec_nr]);
  if (vec_nr == 14) {
    uint32_t page_fault_vaddr;
    asm volatile("movl %%cr2,%0" : "=r"(page_fault_vaddr));
    put_str("\npage fault addr is ");
    put_int(page_fault_vaddr);
  }
  put_str("!!!!!!      exception message end      !!!!!!");
  while (1)
    ;
}

void register_handler(uint8_t vec_nr, intr_handler function) {
  idt_table[vec_nr] = function;
}

/**
 * exception_init - initialize idt-table
 *
 * This function sets the default interrupt handler function for all
 * interrupts to general_intr_handler.
 */
static void exception_init() {
  int i;
  for (i = 0; i < IDT_DESC_COUNT; ++i) {
    idt_table[i] = general_intr_handler;
    intr_name[i] = "unknown";
  }

  intr_name[0] = "#DE Divide Error";
  intr_name[1] = "#DB Debug";
  intr_name[2] = "NMI Interrupt";
  intr_name[3] = "#BP BreakPoint";
  intr_name[4] = "#OF Overflow";
  intr_name[5] = "#BR BOUND Range Exceeded";
  intr_name[6] = "#UD Undefined Opcode";
  intr_name[7] = "#NM Device Not Available";
  intr_name[8] = "#DF Double Fault";
  intr_name[9] = "#MF CoProcessor Segment Overrun";
  intr_name[10] = "#TS Invalid TSS";
  intr_name[11] = "#NP Segment Not Present";
  intr_name[12] = "#SS Stack Segment Fault";
  intr_name[13] = "#GP General  Protection";
  intr_name[14] = "#PF Page Fault";
  intr_name[16] = "#MF x87 FPU Floating-Point Error";
  intr_name[17] = "#AC Alignment Check";
  intr_name[18] = "#MC Machine Check";
  intr_name[19] = "#XM SIMD Floating-Point Exception";
}

/*
 * idt-init - initialize IDT and interrupt agent 8259A chips
 *
 * This function completes the initialization of IDT and chips by calling
 * idt_desc_init and pic_init.
 */
void idt_init() {
  put_str("idt_init start\n");

  idt_desc_init();
  exception_init();
  pic_init();

  /*
   * load the address of IDT into IDTR.
   *
   * the 2 conversions to 32-bit pointers here are to avoid incorrect sign
   * extension.
   */
  uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
  asm volatile("lidt %0" ::"m"(idt_operand));

  put_str("idt_init done\n");
}

/**
 * intr_get_status - Get current interrupt status by checking flag IF
 *
 * Return: interrupt status. The relevant enumeration variables are defined in
 * interrupt.h.
 */
enum intr_status intr_get_status() {
  uint32_t eflags = 0;
  GET_EFLAGS(eflags);
  return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

/**
 * intr_enable - enable interrupt
 *
 * This function uses the `std` instruction to set the bit "if" in the eflag
 * register to 1.
 *
 * Return: old interrupt status.
 */
enum intr_status intr_enable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
  } else {
    old_status = INTR_OFF;
    asm volatile("sti");
  }
  return old_status;
}

/**
 * intr_disable - disable interrupt
 *
 * This function uses the `cli` instruction to set the bit "if" in the eflag
 * register to 0.
 *
 * Return: old interrupt status.
 */
enum intr_status intr_disable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
    asm volatile("cli" : : : "memory");
  } else {
    old_status = INTR_OFF;
  }
  return old_status;
}
/**
 * intr_set_status - set interrupt status to the value of parameter status
 * @status: the interrupt status to set.
 *
 * Return: old interrupt status
 */
enum intr_status intr_set_status(enum intr_status status) {
  return status & INTR_ON ? intr_enable() : intr_disable();
}
