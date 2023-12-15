/*
 * Author: Zhang Xun
 * Time: 2023-11-26
 */
#include "process.h"
#include "console.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "string.h"
#include "thread.h"
#include "tss.h"
#include "userprog.h"

extern void intr_exit(void);
extern struct list thread_ready_list;
extern struct list thread_all_list;

/*
 * start_process - Build the context of the user process. This function is the
 * address of the "return from interrupt"
 * @_filename: filename of user process and also name of process
 *
 * This function initialize intr_stack for the user process, which is the
 * context of the user process
 */
void start_process(void *_filename) {
  void *function = _filename;
  struct task_struct *cur_thread = running_thread();
  /* let self_kstack skip over thread_stack and point to intr_stack  */
  cur_thread->self_kstack += sizeof(struct thread_stack);
  struct intr_stack *proc_stack = (struct intr_stack *)cur_thread->self_kstack;

  /* initialize intr_stack for process. as to general registers, they are unused
   * for now, so initialize them to 0  */
  proc_stack->edi = proc_stack->esi = 0;
  proc_stack->ebp = proc_stack->esp_dummy = 0;
  proc_stack->ebx = proc_stack->edx = 0;
  proc_stack->ecx = proc_stack->eax = 0;

  proc_stack->gs = 0;
  proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;

  proc_stack->cs = SELECTOR_U_CODE;
  proc_stack->eip = function;

  proc_stack->eflags = (EFLAGS_IF_1 | EFLAGS_IOPL_0 | EFLAGS_MBS);

  proc_stack->ss = SELECTOR_U_DATA;

  /* Allocate stack with priority 3 for user process */
  proc_stack->esp =
      (void *)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PAGE_SIZE);

  /* Jumping to the interrupt exit so that CPU can complete the transfer from
   * high privilege level (os kernel) to low privilege level (user process)
   * through interrupt
   */
  asm volatile("movl %0,%%esp; jmp intr_exit" ::"g"(proc_stack) : "memory");
}

/**
 * page_dir_activate() - Activate the page directory for the given thread.
 * @pthread: Pointer to the thread whose page directory needs to be activated.
 *
 * This function loads the appropriate page directory address into CR3
 * register. If the thread is a user process, it uses its own page directory;
 * otherwise, it uses the kernel's page directory.
 */
void page_dir_activate(struct task_struct *pthread) {
  /* kernel thread  */
  uint32_t page_dir_phy_addr = 0x100000;
  /* thread or process ? */
  if (pthread->pg_dir != NULL) {
    /* process, Swithc PD  */
    page_dir_phy_addr = addr_v2p((uint32_t)pthread->pg_dir);
  }
  asm volatile("movl %0, %%cr3" ::"r"(page_dir_phy_addr) : "memory");
}

/**
 * process_activate() - Activate the process and update TSS.
 * @pthread: Pointer to the process to be activated.
 *
 * This function updates the ESP0 stack pointer in the TSS for the given
 * process and activates its page directory by calling page_dir_activate.
 */
void process_activate(struct task_struct *pthread) {
  ASSERT(pthread != NULL);
  page_dir_activate(pthread);
  if (pthread->pg_dir) {
    update_tss_esp(pthread);
  }
}

/**
 * create_page_dir() - Create a page directory for a user process.
 *
 * Allocates and initializes a new page directory for a user process. It copies
 * kernel entries to the new page directory and sets up a self-reference.
 * Returns a pointer (vaddr) to the newly created page directory.
 */
uint32_t *create_page_dir(void) {
  /* create page directory for user process  */
  uint32_t *user_page_dir_vaddr = get_kernel_pages(1);
  if (user_page_dir_vaddr == NULL) {
    console_put_str("create_page_dir: get_kernel_pages failed!");
    return NULL;
  }

  /* create kernel entrance in user virtual address space by mapping the upper
   * 1GB (PDE.768~ PDE.1023) to the kernel physical address space (1024/4=256
   * entries in PD, which is 1GB)
   */
  memcpy((uint32_t *)((uint32_t)user_page_dir_vaddr + 0x300 * 4),
         (uint32_t *)(0xfffff000 + 0x300 * 4), 1024);

  /* Let the last item of the user's PDE point to the page directory itself */
  uint32_t user_page_dir_phy_addr = addr_v2p((uint32_t)user_page_dir_vaddr);
  user_page_dir_vaddr[1023] =
      user_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
  return user_page_dir_vaddr;
}

/**
 * create_user_vaddr_bitmap() - Create a bitmap for managing user process's
 * virtual addresses.
 * @user_prog: Pointer to the user process's task struct.
 *
 * Initializes the bitmap used for managing virtual addresses of a user
 * process, starting from a predefined user virtual address (0x8048000).
 */
void create_user_vaddr_bitmap(struct task_struct *user_prog) {
  user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;

  /* the number of physical pages required for the user's bitmap   */
  uint32_t bitmap_pg_cnt =
      DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
  user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
  user_prog->userprog_vaddr.vaddr_bitmap.bmap_bytes_len =
      (0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8;
  bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/**
 * process_execute() - Creates a new user process.
 * @filename: Pointer to the filename of the process to (ready to) execute.
 * @name: Name of the process.
 *
 * This function creates a new user process, initializes its thread structure,
 * and adds it to the ready and all threads list. It also creates necessary
 * structures for user process like the user address space bitmap and the page
 * directory.
 */
void process_execute(void *filename, char *name) {
  /* create PCB for user process (a thread essentially)*/
  struct task_struct *user_thread = get_kernel_pages(1);
  ASSERT(user_thread != NULL);
  /* initialize the PCB of user process*/
  init_thread(user_thread, name, default_prio);
  /* create bitmap for virtual address space  */
  create_user_vaddr_bitmap(user_thread);
  /* initialize thread stack */
  thread_create(user_thread, start_process, filename);
  /* create user process's page directory for address mapping*/
  user_thread->pg_dir = create_page_dir();

  block_desc_init(user_thread->u_mb_desc_arr);

  /* ready to run  */
  enum intr_status old_status = intr_disable();
  ASSERT(!list_elem_find(&thread_ready_list, &user_thread->general_tag));
  list_append(&thread_ready_list, &user_thread->general_tag);
  ASSERT(!list_elem_find(&thread_all_list, &user_thread->all_list_tag));
  list_append(&thread_all_list, &user_thread->all_list_tag);
  intr_set_status(old_status);
}
