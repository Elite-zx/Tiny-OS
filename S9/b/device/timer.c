/*
 * Author: Xun Morris
 * Time: 2023-11-20
 */

#include "timer.h"
#include "debug.h"
#include "interrupt.h"
#include "io.h"
#include "print.h"
#include "thread.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40

#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

/* the total number of ticks since the interrupt was enabled  */
uint32_t ticks;

/**
 * frequency_set - Initialize programmable Interval Timer Intel 8253
 * @counter_port: as to counter NO.0, this value is 0x40
 * @counter_no: the timer serial number to be set, set to 0 here
 * @rwl: bit Read/Write/Latch of control word in Intel 8253
 * @counter_mode: the working mode of counter, set to 2 here which means
 * periodic counting
 * @counter_value: the initial count value of counter0
 *
 * This function sets appropriate value for counter NO.0 in Intel 8253.
 */
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl,
                          uint8_t counter_mode, uint16_t counter_value) {
  outb(PIT_CONTROL_PORT,
       (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));

  /* low 8 bits of the initial count value of counter0  */
  outb(counter_port, (uint8_t)counter_value);
  /* high 8 bits of the initial count value of counter0  */
  outb(counter_port, (uint8_t)counter_value >> 8);
}

static void intr_time_handler() {
  struct task_struct *cur_thread = running_thread();
  ASSERT(cur_thread->stack_magic == 0x20011124);

  ++cur_thread->elapsed_ticks;
  ++ticks;

  if (cur_thread->ticks == 0) {
    /* time slice of current thread is over */
    schedule();
  } else {
    --cur_thread->ticks;
  }
}

/**
 * timer_init - Initialize timer
 *
 * This function encapsulate function frequency_set. Therefore, the external
 * function can easily call timer_init to complete the Initialization of timer.
 * Moreover, this function re-registers the clock interrupt handler function.
 */
void timer_init() {
  put_str("timer_init start\n");
  frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER0_MODE,
                COUNTER0_VALUE);
  register_handler(0x20, intr_time_handler);
  put_str("timer_init done\n");
}
