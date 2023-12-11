#include "bitmap.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "inode.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "process.h"
#include "stdint.h"
#include "string.h"
#include "thread.h"

extern void intr_exit(void);
extern struct file file_table[MAX_FILES_OPEN];
extern struct list thread_ready_list;
extern struct list thread_all_list;

static int32_t copy_PCB_and_vaddr_bitmap(struct task_struct *child_thread,
                                         struct task_struct *parent_thread) {
  /******** build PCB for child_thread ********/
  memcpy(child_thread, parent_thread, PAGE_SIZE);
  child_thread->pid = fork_pid();
  child_thread->elapsed_ticks = 0;
  child_thread->status = TASK_READY;
  child_thread->ticks = child_thread->priority;
  child_thread->parent_pid = parent_thread->pid;
  child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
  child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
  block_desc_init(child_thread->u_mb_desc_arr);

  /******** build vaddr bitmap for child_thread ********/
  uint32_t bitmap_pg_cnt =
      DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
  void *vaddr_bitmap = get_kernel_pages(bitmap_pg_cnt);
/* copy the bitmap of virtual address pool of parent process to child process    */
  memcpy(vaddr_bitmap, child_thread->userprog_vaddr.vaddr_bitmap.bits,
         PAGE_SIZE * bitmap_pg_cnt);
  child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_bitmap;

  ASSERT(strlen(child_thread->name) < 11);
  strcat(child_thread->name, "_fork");
  return 0;
}

static void copy_body_and_userstack(struct task_struct *child_thread,
                                    struct task_struct *parent_thread,
                                    void *buf_page) {

  uint8_t *vaddr_bitmap = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
  uint32_t btmp_bytes_len =
      parent_thread->userprog_vaddr.vaddr_bitmap.bmap_bytes_len;
  uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
  uint32_t idx_byte = 0;
  uint32_t idx_bit;
  uint32_t data_page_vaddr = 0;

  /******** find pages with data in parent process and copy them page by page to
   * the child process.********/

  /* heap and stack makes the data in the process discontinuous */
  while (idx_byte < btmp_bytes_len) {
    if (vaddr_bitmap[idx_byte] != 0) {
      idx_bit = 0;
      while (idx_bit < 8) {
        /* traverse the bits within the byte  */
        if (((BITMAP_MASK << idx_bit) & vaddr_bitmap[idx_byte]) != 0) {
          data_page_vaddr = (idx_byte * 8 + idx_bit) * PAGE_SIZE + vaddr_start;

          /******** Copy data from one process to another using the kernel
           * buffer buf_page for transfer ********/

          memcpy(buf_page, (void *)data_page_vaddr, PAGE_SIZE);

          /* switch page table from parent process to child process, to make
           * sure that the pte and pde of the page allocated to the
           * child process are in the child process's page table  */
          page_dir_activate(child_thread);

          get_page_to_vaddr_without_bitmap(PF_USER, data_page_vaddr);

          memcpy((void *)data_page_vaddr, buf_page, PAGE_SIZE);

          /* restore page table, to find the next page with data  */
          page_dir_activate(parent_thread);
        }
        /* next idx  */
        idx_bit++;
      }
    }
    /* next byte in parent vaddr bitmap  */
    idx_byte++;
  }
}

static int32_t build_child_kernel_stack(struct task_struct *child_thread) {
  /******** set return value (PID) to zero ********/
  struct intr_stack *intr_stack_0 =
      (struct intr_stack *)((uint32_t)child_thread + PAGE_SIZE -
                            sizeof(struct intr_stack));
  intr_stack_0->eax = 0;

  /******** build thread_stack  ********/
  uint32_t *ret_addr_in_thread_stack = (uint32_t *)intr_stack_0 - 1;
  uint32_t *esi_ptr_in_thread_stack = (uint32_t *)intr_stack_0 - 2;
  uint32_t *edi_ptr_in_thread_stack = (uint32_t *)intr_stack_0 - 3;
  uint32_t *ebx_ptr_in_thread_stack = (uint32_t *)intr_stack_0 - 4;
  /* the address of ebp is the stack address of the child process */
  uint32_t *ebp_ptr_in_thread_stack = (uint32_t *)intr_stack_0 - 5;

  *ret_addr_in_thread_stack = (uint32_t)intr_exit;

  *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack =
      *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;

  /* Let self_kstack point to the top of the thread stack  */
  child_thread->self_kstack = ebp_ptr_in_thread_stack;
  return 0;
}

static void update_inode_open_cnt(struct task_struct *thread) {
  int32_t local_fd = 3;
  int32_t global_fd = 0;
  while (local_fd < MAX_FILES_OPEN_PER_PROC) {
    global_fd = thread->fd_table[local_fd];
    ASSERT(global_fd < MAX_FILES_OPEN);
    if (global_fd != -1) {
      file_table[global_fd].fd_inode->i_open_cnt++;
    }
    local_fd++;
  }
}

static int32_t copy_process(struct task_struct *child_thread,
                            struct task_struct *parent_thread) {
  void *buf_page = get_kernel_pages(1);
  if (buf_page == NULL) return -1;

  if (copy_PCB_and_vaddr_bitmap(child_thread, parent_thread) == -1) return -1;

  child_thread->pg_dir = create_page_dir();
  if (child_thread->pg_dir == NULL) return -1;

  copy_body_and_userstack(child_thread, parent_thread, buf_page);
  build_child_kernel_stack(child_thread);
  update_inode_open_cnt(child_thread);
  mfree_page(PF_KERNEL, buf_page, 1);
  return 0;
}

pid_t sys_fork() {
  struct task_struct *parent_thread = running_thread();
  struct task_struct *child_thread = get_kernel_pages(1);

  if (child_thread == NULL)  return -1;

  ASSERT(INTR_OFF==intr_get_status()&&parent_thread->pg_dir!=NULL);

  if (copy_process(child_thread, parent_thread) ==-1) return -1;

  ASSERT(!list_elem_find(&thread_ready_list, &child_thread->general_tag));
  list_append(&thread_ready_list, &child_thread->general_tag);
  ASSERT(!list_elem_find(&thread_all_list, &child_thread->all_list_tag));
  list_append(&thread_all_list, &child_thread->all_list_tag);

/* return the pid of child process for parent process  */
  return  child_thread->pid;
}
