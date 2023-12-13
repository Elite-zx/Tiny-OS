/*
 * Author: Zhang Xun
 * Time: 2023-11-30
 */
#include "memory.h"
#include "bitmap.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "print.h"
#include "stdint.h"
#include "string.h"
#include "sync.h"
#include "thread.h"

/* virtual address for kernel bitmap  */
#define MEM_BITMAP_BASE 0xc009a000

/* The kernel's virtual address starts from 3G and needs to spans the beginning
 * and used 1MB, that is, 0xc0000000 + 0x00100000= 0xc0100000*/
#define KERNEL_HEAP_START 0xc0100000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/**
 * struct pool - Represents a physical memory pool.
 * @pool_bitmap: Bitmap to track the allocation status of pages in the pool.
 * @phy_addr_start: The starting physical address of the memory pool.
 * @pool_size: The total size of the memory pool.
 *
 * This structure is used to manage a physical memory pool, either for the
 * kernel or user space. It includes a bitmap to efficiently track which
 * pages are free or allocated, as well as the starting address and total
 * size of the memory pool.
 */
struct pool {
  struct bitmap pool_bitmap;
  uint32_t phy_addr_start;
  uint32_t pool_size;
  struct lock _lock;
};

struct pool kernel_pool, user_pool;

/* virtual memory pool of kernel */
struct virtual_addr kernel_vaddr;

/**
 * arena - arena meta info
 * @desc: ponter to memory block descriptor
 * @cnt: if large is true, cnt represents the number of page frames, otherwise
 * it represents the number of free memory blocks in arena
 * @large_mb: related with cnt
 */
struct arena {
  struct mem_block_desc *desc;
  uint32_t cnt;
  bool large_mb;
};

struct mem_block_desc k_mb_desc_arr[MB_DESC_CNT];

/**
 * mem_pool_init() - Initializes the physical and virtual memory pools for
 * kernel and user.
 * @all_mem: The total physical memory size.
 *
 * This function initializes memory pools for both the kernel and user.
 * It calculates and divides the available memory between the kernel and user
 * space, accounting for the memory already used by the kernel. It initializes
 * the bitmaps for these pools to keep track of free and used pages. The
 * function also sets up the starting addresses and sizes for the kernel and
 * user memory pools. Additionally, it initializes the virtual address pool for
 * the kernel.
 *
 * Context: This function should be called during system initialization to set
 * up memory pools for the kernel and user space.
 */
static void mem_pool_init(uint32_t all_mem) {
  put_str("  mem_pool_init start\n");
  lock_init(&kernel_pool._lock);
  lock_init(&user_pool._lock);

  /* 1 PDT + 255 PTs = 4KB*256=1024KB=1MB (0x100000B) */
  uint32_t page_table_size = PAGE_SIZE * 256;

  /* 0x100000 is the low 1MB physical space used by the kernel */
  /* so the value of used_mem = 2MB (0x200000)*/
  uint32_t used_mem = page_table_size + 0x100000;
  uint32_t free_mem = all_mem - used_mem;

  /* ignore physical memory smaller than PAGE_SIZE */
  uint16_t all_free_pages = free_mem / PAGE_SIZE;

  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;

  uint32_t kernel_bitmap_len = kernel_free_pages / 8;
  uint32_t user_bitmap_len = user_free_pages / 8;

  uint32_t kernel_pool_start = used_mem;
  uint32_t user_pool_start = kernel_pool_start + kernel_free_pages * PAGE_SIZE;

  kernel_pool.phy_addr_start = kernel_pool_start;
  kernel_pool.pool_size = kernel_free_pages * PAGE_SIZE;
  kernel_pool.pool_bitmap.bmap_bytes_len = kernel_bitmap_len;

  user_pool.phy_addr_start = user_pool_start;
  user_pool.pool_size = user_free_pages * PAGE_SIZE;
  user_pool.pool_bitmap.bmap_bytes_len = user_bitmap_len;

  kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;
  user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kernel_bitmap_len);

  put_str("    kernel_pool_bitmap_start:");
  put_int((int)kernel_pool.pool_bitmap.bits);
  put_str(" kernel_pool_phy_start:");
  put_int(kernel_pool.phy_addr_start);
  put_str("\n");

  put_str("    user_pool_bitmap_start:");
  put_int((int)user_pool.pool_bitmap.bits);
  put_str(" user_pool_phy_start:");
  put_int(user_pool.phy_addr_start);
  put_str("\n");

  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);

  kernel_vaddr.vaddr_bitmap.bmap_bytes_len = kernel_bitmap_len;
  kernel_vaddr.vaddr_bitmap.bits =
      (void *)(MEM_BITMAP_BASE + kernel_bitmap_len + user_bitmap_len);

  kernel_vaddr.vaddr_start = KERNEL_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);
  put_str("  mem_pool_init done\n");
}

/**
 * block_desc_init() - Initialize an array of memory block descriptors.
 * @desc_array: Array of memory block descriptors to initialize.
 *
 * This function initializes each memory block descriptor in the given array.
 * It sets up the block size, calculates the number of blocks per arena, and
 * initializes the free list for each descriptor. The block sizes are set
 * starting from 16 bytes and doubled for each subsequent descriptor.
 *
 * Context: This function is used to prepare for memory allocation operations,
 *          specifically for the malloc function. It should be called during
 *          memory system initialization.
 */
void block_desc_init(struct mem_block_desc *k_mb_desc_arr) {
  uint16_t desc_idx, _block_size = 16;
  for (desc_idx = 0; desc_idx < MB_DESC_CNT; desc_idx++) {
    k_mb_desc_arr[desc_idx].block_size = _block_size;
    k_mb_desc_arr[desc_idx].block_per_arena =
        (PAGE_SIZE - sizeof(struct arena)) / _block_size;
    list_init(&k_mb_desc_arr[desc_idx].free_list);
    _block_size *= 2;
  }
}

/**
 * mem_init() - Entry point for memory management initialization.
 *
 * This function marks the start of memory initialization. It first prints a
 * message to indicate the start of memory initialization. It then reads the
 * total memory size and initializes the memory pool with this size. Finally,
 * it initializes the array of memory block descriptors, which is essential for
 * the malloc function, and prints a completion message.
 *
 * Context: This function is crucial for setting up the memory management
 * system. It initializes the memory pool and prepares the memory block
 *          descriptors for dynamic memory allocation.
 * Return: This function does not return a value.
 */
void mem_init() {
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));
  mem_pool_init(mem_bytes_total);
  block_desc_init(k_mb_desc_arr);
  put_str("mem_init done\n");
}

/**
 * vaddr_get - Request pg_cnt virtual pages from virtual memory pool pf.
 * @pf: virtual memory pool.
 * @pg_cnt: number of virtual pages to be applyied for.
 *
 * Return: the starting address of the virtual page if successful, otherwise
 * returns NULL.
 */
static void *vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
  int vaddr_start = 0, free_bit_idx_start = -1;
  uint32_t cnt = 0;

  if (pf == PF_KERNEL) {
    free_bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
    if (free_bit_idx_start == -1)
      return NULL;
    while (cnt < pg_cnt) {
      bitmap_set(&kernel_vaddr.vaddr_bitmap, free_bit_idx_start + cnt++, 1);
    }
    vaddr_start = kernel_vaddr.vaddr_start + free_bit_idx_start * PAGE_SIZE;
  } else {
    /* for user process*/
    struct task_struct *cur = running_thread();
    free_bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
    if (free_bit_idx_start == -1)
      return NULL;
    while (cnt < pg_cnt) {
      bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, free_bit_idx_start + cnt++,
                 1);
    }
    vaddr_start =
        cur->userprog_vaddr.vaddr_start + free_bit_idx_start * PAGE_SIZE;
    ASSERT((uint32_t)vaddr_start < (0xc0000000 - PAGE_SIZE));
  }
  return (void *)vaddr_start;
}

/**
 * pte_ptr - Calculates the virtual address of the page table entry for a given
 * virtual address.
 * @vaddr: The virtual address for which to find the corresponding page table
 * entry.
 *
 * This function computes the virtual address of the page table entry (PTE)
 * corresponding to the provided virtual address (`vaddr`). It utilizes the last
 * entry in the page (The high 10 bits of vaddr should be 1 to access this PDE)
 * directory to access the page directory itself, thus enabling the calculation
 * of the virtual address that can be used to access the appropriate PTE within
 * the page table. See Page.391 for details.
 *
 * Return: The virtual address of the page table entry corresponding to the
 * given virtual address.
 */
uint32_t *pte_ptr(uint32_t vaddr) {
  uint32_t *pte = (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +
                               PTE_IDX(vaddr) * 4);
  return pte;
}

/**
 * pde_ptr - Calculates the virtual address of the page directory entry for a
 * given virtual address.
 * @vaddr: The virtual address for which to find the corresponding page
 * directory entry.
 *
 * This function computes the virtual address of the page directory entry (PDE)
 * corresponding to the provided virtual address (`vaddr`). It uses the last
 * entry of the page directory to access the directory itself, enabling the
 * determination of the virtual address that can be used to access the correct
 * PDE. The calculation is done by starting from a fixed high memory location
 * (0xfffff000) which represents the starting address of PDE and applying an
 * offset based on the `vaddr` to reach the specific PDE. See Page.392 for
 * details.
 *
 * Return: The virtual address of the page directory entry corresponding to the
 * given virtual address.
 */
uint32_t *pde_ptr(uint32_t vaddr) {
  uint32_t *pde = (uint32_t *)((0xfffff000) + PDE_IDX(vaddr) * 4);
  return pde;
}

/**
 * palloc - Allocates a physical page from the given memory pool.
 * @m_pool: A pointer to the memory pool from which to allocate the page.
 *
 * Allocates a single physical page from the specified physical memory pool.
 * The function searches for a free bit in the pool's bitmap, marks it as used,
 * and returns the physical address of the allocated page.
 *
 * Return: A pointer to the start of the allocated physical page, or NULL if
 * no free page is available.
 */
static void *palloc(struct pool *m_pool) {
  int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
  if (bit_idx == -1)
    return NULL;
  bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
  uint32_t page_phy_addr = m_pool->phy_addr_start + bit_idx * PAGE_SIZE;
  return (void *)page_phy_addr;
}

/**
 * page_table_Add() - Establishes a mapping between a virtual address and a
 * physical address.
 * @_vaddr: The virtual address.
 * @_page_phy_addr: The physical address.
 *
 * This function sets up the page table entry (PTE) for a given virtual address.
 * It involves checking if the page directory entry (PDE) for the given virtual
 * address exists. If the PDE exists, indicating the presence of a page table,
 * the function proceeds to create a PTE. It ensures that the PTE does not
 * already exist before setting it up. If the PDE does not exist, it allocates a
 * physical page for the page table int kernel space, initializes this new page
 * table (set to 0), and then creates the PTE. The function uses bit
 * manipulation to set the proper flags in the PDE and PTE for the mapping.
 *
 * Context: This function should be called in the context where it is safe to
 * modify the page tables. It assumes that the kernel page pool has been
 * initialized.
 * Return: This function does not return a value.
 *
 * Note: This function uses ASSERT to ensure that the PTE does not already exist
 * when setting up a new PTE. It also performs necessary bit manipulation for
 * setting the flags in the page table entries.
 */
static void page_table_add(void *_vaddr, void *_page_phy_addr) {
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t page_phy_addr = (uint32_t)_page_phy_addr;
  uint32_t *pde = pde_ptr(vaddr);
  uint32_t *pte = pte_ptr(vaddr);

  /* check if pde exists by bit present  */
  if (*pde & 0x00000001) {
    /* pde exists, which means the page table exists, so just create pte  */

    /* make sure that pte does not exist*/
    ASSERT(!(*pte & 0x00000001));

    *pte = (page_phy_addr | PG_US_U | PG_RW_W | PG_P_1);
  } else {
    /* pde does not exists, which means the page table does not exists, so apply
     * for a physical page as a page table in kernel_pool  */
    uint32_t pde_phy_addr = (uint32_t)palloc(&kernel_pool);
    *pde = (pde_phy_addr | PG_US_U | PG_RW_W | PG_P_1);
    /* memset requires a virtual address. Get the virtual address of the page
     * table through the value of pte  */
    memset((void *)((int)pte & 0xfffff000), 0, PAGE_SIZE);

    *pte = (page_phy_addr | PG_US_U | PG_RW_W | PG_P_1);
  }
}

/**
 * malloc_page() - Allocates a specified number of page spaces.
 * @pf: The pool flag indicating which memory pool to use.
 * @pg_cnt: The number of pages to allocate.
 *
 * This function allocates 'pg_cnt' pages of virtual memory and establishes the
 * mapping between the virtual pages and physical pages. It ensures that the
 * request does not exceed the total physical memory size. The function first
 * allocates virtual pages, then for each virtual page, it allocates a
 * corresponding physical page and sets up the page table entries (PTEs, and
 * possibly PDEs). If the allocation fails at any point, the function returns
 * NULL.
 *
 * Context: Used for allocating memory in either the kernel or user space,
 * depending on the pool flag.
 * Return: Returns the start address of the allocated virtual pages if
 * successful, otherwise NULL.
 */
void *malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
  /* Make sure that the memory size represented by pg_cnt does not exceed the
   * total physical memory size. 15MB/4KB = 3840*/
  ASSERT(pg_cnt > 0 && pg_cnt < 3840);
  /* allocate virtual pages */
  void *vaddr_start = vaddr_get(pf, pg_cnt);
  if (vaddr_start == NULL)
    return NULL;

  uint32_t vaddr = (uint32_t)vaddr_start;
  uint32_t cnt = pg_cnt;
  struct pool *mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;

  /* Allocate physical pages in corresponding pool, that is, establish a mapping
   * relationship between virtual pages and physical pages, that is, create PTE
   * (and PDE possibly) */
  while (cnt-- > 0) {
    void *page_phy_addr = palloc(mem_pool);
    if (page_phy_addr == NULL)
      return NULL;
    page_table_add((void *)vaddr, page_phy_addr);
    vaddr += PAGE_SIZE;
  }
  return vaddr_start;
}

/**
 * get_kernel_pages() - Allocates kernel pages and initializes them to zero.
 * @pg_cnt: The number of pages to allocate.
 *
 * This function is a wrapper for malloc_page specifically for allocating kernel
 * memory. It allocates 'pg_cnt' pages in the kernel space using malloc_page. If
 * successful, it then initializes the allocated memory to zero. This is
 * particularly used for kernel space memory allocations where initialization is
 * required.
 *
 * Context: Used when kernel space memory is required, and the allocated memory
 * needs to be initialized to zero.
 * Return: Returns the start address of the allocated and initialized virtual
 * pages if successful, otherwise NULL.
 */
void *get_kernel_pages(uint32_t pg_cnt) {
  void *vaddr = malloc_page(PF_KERNEL, pg_cnt);
  if (vaddr != NULL)
    memset(vaddr, 0, pg_cnt * PAGE_SIZE);
  return vaddr;
}

/**
 * get_user_page - Allocates user space pages
 * @pg_cnt: The number of 4K pages to allocate
 * Return: Virtual address to the allocated user space memory
 *
 * Allocates 'pg_cnt' number of 4K pages in user space, initializes the
 * allocated space to zero, and returns the virtual address to the allocated
 * space. Ensures mutual exclusion during allocation by acquiring a lock on the
 * user memory pool.
 */
void *get_user_page(uint32_t pg_cnt) {
  lock_acquire(&user_pool._lock);
  void *vaddr = malloc_page(PF_USER, pg_cnt);
  if (vaddr != NULL)
    memset(vaddr, 0, pg_cnt * PAGE_SIZE);
  lock_release(&user_pool._lock);
  return vaddr;
}

/**
 * get_a_page - Maps a virtual address to a physical page
 * @pf: Pool flag indicating whether the page is for user or kernel
 * @vaddr: Virtual address to map to
 * Return: Virtual address 'vaddr' on success, NULL on failure
 *
 * Maps a given virtual address 'vaddr' to a physical page from the specified
 * pool 'pf' (user or kernel). The function first acquires a lock on the memory
 * pool, calculates the bitmap index from the virtual address, sets the
 * corresponding bit in the bitmap to indicate the page is used, allocates a
 * physical page, and adds the mapping between the virtual address and the
 * physical page. Releases the lock before returning. If the physical page
 * allocation fails, returns NULL.
 */
void *get_a_page(enum pool_flags pf, uint32_t vaddr) {
  struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->_lock);
  struct task_struct *cur_thread = running_thread();
  int32_t bit_idx = -1;

  if (cur_thread->pg_dir != NULL && pf == PF_USER) {
    bit_idx = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PAGE_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
  } else if (cur_thread->pg_dir == NULL && pf == PF_KERNEL) {
    bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
  } else {
    PANIC("Unable to establish mapping between pf and vaddr");
  }
  void *page_phy_addr = palloc(mem_pool);
  if (page_phy_addr == NULL)
    return NULL;
  page_table_add((void *)vaddr, page_phy_addr);
  lock_release(&mem_pool->_lock);
  return (void *)vaddr;
}

/**
 * addr_v2p - Converts a virtual address to a physical address
 * @vaddr: The virtual address to be converted
 * Return: The corresponding physical address
 *
 * This function converts a given virtual address to its corresponding physical
 * address using page table entries. It first locates the page table entry for
 * the given virtual address and then extracts the physical address from it. The
 * function combines the high 20 bits of the physical page frame address
 * (extracted from the page table entry) with the low 12 bits of the original
 * virtual address to form the complete physical address.
 */
uint32_t addr_v2p(uint32_t vaddr) {
  uint32_t *pte_phy_addr = pte_ptr(vaddr);
  return ((*pte_phy_addr & 0xfffff000) + (vaddr & 0x00000fff));
}

/**
 * arena2block() - Get the address of a memory block within an arena.
 * @a: Pointer to the arena structure.
 * @idx: Index of the memory block within the arena.
 *
 * This function calculates and returns the address of the memory block located
 * at the specified index within the given arena. It accounts for the size of
 * the arena structure and the size of each block within the arena.
 *
 * Context: Useful in memory management for accessing specific memory blocks
 * within an arena, particularly when handling memory allocation and
 * deallocation.
 *
 * Return: Address of the specified memory block within the arena.
 */
static struct mem_block *arena_2_block(struct arena *a, uint32_t idx) {
  return (struct mem_block *)((uint32_t)a + sizeof(struct arena) +
                              idx * a->desc->block_size);
}

/**
 * block2arena() - Find the arena address corresponding to a memory block.
 * @b: Pointer to the memory block.
 *
 * This function computes and returns the starting address of the arena that
 * contains the given memory block. It uses the memory block address to
 * backtrack to the start of the arena.
 *
 * Context: Utilized in memory management to identify the arena associated with
 * a specific memory block, especially during memory freeing operations.
 *
 * Return: Address of the arena containing the given memory block.
 */
static struct arena *block_2_arena(struct mem_block *mb) {
  return (struct arena *)((uint32_t)mb & 0xfffff000);
}

/**
 * sys_malloc() - Allocate memory of the given size for user or kernel.
 * @_size: The size of memory to be allocated.
 */
void *sys_malloc(uint32_t _size) {
  enum pool_flags PF;
  struct pool *mem_pool;
  uint32_t pool_size;
  struct mem_block_desc *desc;
  struct task_struct *cur_thread = running_thread();

  /* kernel thread or user process  */
  if (cur_thread->pg_dir == NULL) {
    PF = PF_KERNEL;
    pool_size = kernel_pool.pool_size;
    mem_pool = &kernel_pool;
    desc = k_mb_desc_arr;
  } else {
    PF = PF_USER;
    pool_size = user_pool.pool_size;
    mem_pool = &user_pool;
    desc = cur_thread->u_mb_desc_arr;
  }

  if (!(_size < pool_size))
    return NULL;

  struct arena *a = NULL;
  struct mem_block *b = NULL;
  lock_acquire(&mem_pool->_lock);

  if (_size > 1024) {
    uint32_t pg_cnt = DIV_ROUND_UP(_size + sizeof(struct arena), PAGE_SIZE);
    a = malloc_page(PF, pg_cnt);
    if (a != NULL) {
      memset(a, 0, pg_cnt * PAGE_SIZE);
      a->desc = NULL;
      a->cnt = pg_cnt;
      a->large_mb = true;
      lock_release(&mem_pool->_lock);
      return (void *)(a + 1);
    } else {
      lock_release(&mem_pool->_lock);
      return NULL;
    }
  } else {
    /* find proper memory block from small to large  */
    uint8_t desc_idx;
    for (desc_idx = 0; desc_idx < MB_DESC_CNT; desc_idx++) {
      if (_size <= desc[desc_idx].block_size)
        break;
    }
    if (list_empty(&desc[desc_idx].free_list)) {
      /* new arena */
      a = malloc_page(PF, 1);
      if (a == NULL) {
        lock_release(&mem_pool->_lock);
        return NULL;
      }
      memset(a, 0, PAGE_SIZE);
      a->desc = &desc[desc_idx];
      a->large_mb = false;
      a->cnt = desc[desc_idx].block_per_arena;

      /* Divide memory blocks in page frames  (arena)  */
      uint32_t block_idx;
      enum intr_status old_status = intr_disable();
      for (block_idx = 0; block_idx < a->desc->block_per_arena; block_idx++) {
        b = arena_2_block(a, block_idx);
        ASSERT(!list_elem_find(&a->desc->free_list, &b->free_elem));
        list_append(&a->desc->free_list, &b->free_elem);
      }
      intr_set_status(old_status);
    }
    /* now! allocate free memory block from free_list which maintained by memory
     * block descriptor*/

    /* get the address of target free memory block b from its member free_elem*/
    b = elem2entry(struct mem_block, free_elem,
                   list_pop(&desc[desc_idx].free_list));
    memset(b, 0, desc[desc_idx].block_size);
    a = block_2_arena(b);
    --a->cnt;
    lock_release(&mem_pool->_lock);
    return (void *)b;
  }
}

/**
 * pfree() - Recycle a physical address back to the physical memory pool.
 * @pg_phy_addr: The physical address to be recycled.
 *
 * This function recycles a given physical address back into the appropriate
 * physical memory pool. It determines whether the address belongs to the user
 * or kernel physical memory pool and clears the corresponding bit in the bitmap
 * to mark it as free.
 *
 * Context: Used for managing physical memory allocation by keeping track of
 *          allocated and free memory blocks.
 */
void pfree(uint32_t page_phy_addr) {
  struct pool *mem_pool;
  uint32_t bit_idx = 0;
  mem_pool =
      (page_phy_addr >= user_pool.phy_addr_start) ? &user_pool : &kernel_pool;
  bit_idx = (page_phy_addr - mem_pool->phy_addr_start) / PAGE_SIZE;
  bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/**
 * page_table_pte_remove() - Remove the mapping of a virtual address in the page
 * table.
 * @vaddr: The virtual address whose mapping is to be removed.
 *
 * This function removes the mapping of a specific virtual address in the page
 * table by clearing its present bit. It then updates the TLB (Translation
 * Lookaside Buffer) to ensure the changes are immediately effective.
 *
 * Context: Invoked when there's a need to unmap a virtual address, usually as
 * part of freeing or reallocating memory.
 */
static void page_table_pte_remove(uint32_t vaddr) {
  uint32_t *pte = pte_ptr(vaddr);
  *pte &= PG_P_0;
  /* update TLB entry  */
  asm volatile("invlpg %0" ::"m"(vaddr) : "memory");
}

/**
 * vaddr_remove() - Free a range of virtual addresses in the virtual address
 * pool.
 * @pf: The pool flag indicating the type of memory pool.
 * @_vaddr: The starting virtual address of the range to free.
 * @pg_cnt: The number of pages to free starting from _vaddr.
 *
 * This function frees a continuous range of virtual pages from the specified
 * virtual address pool. It clears the bits in the bitmap corresponding to these
 * addresses, effectively marking them as free. It handles both kernel and user
 * virtual memory pools.
 *
 * Context: Used in the process of deallocating virtual memory, particularly
 * when freeing multiple contiguous virtual pages.
 */
static void vaddr_remove(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt) {
  uint32_t allocated_bit_idx_start = -1;
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t cnt = 0;
  if (pf == PF_KERNEL) {
    allocated_bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
    while (cnt < pg_cnt) {
      bitmap_set(&kernel_vaddr.vaddr_bitmap, allocated_bit_idx_start + cnt++,
                 0);
    }
  } else {
    /* for user  */
    struct task_struct *cur_thread = running_thread();
    allocated_bit_idx_start =
        (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PAGE_SIZE;
    while (cnt < pg_cnt) {
      bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,
                 allocated_bit_idx_start + cnt++, 0);
    }
  }
}

/**
 * mfree_page() - Free a range of physical pages.
 * @pf: The pool flag indicating the type of memory pool.
 * @_vaddr: The starting virtual address of the pages to free.
 * @pg_cnt: The number of physical pages to free.
 *
 * This function frees a specified number of physical pages starting from a
 * given virtual address. It translates the virtual address to a physical
 * address and ensures it falls within the correct memory pool. The function
 * then recycles each physical page and removes the corresponding virtual
 * address mapping.
 *
 * Context: Integral for memory management, specifically for freeing a block of
 *          physical memory and its associated virtual mappings.
 */
void mfree_page(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt) {
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t cnt = 0;
  ASSERT(pg_cnt >= 1 && vaddr % PAGE_SIZE == 0);
  uint32_t page_phy_addr = addr_v2p(vaddr);

  /* Exclude low-end 1MB kernel, 1KB page directory table, and 1KB page table,
   * that is, 0x100000+0x001000+0x001000=102000
   */
  ASSERT((page_phy_addr % PAGE_SIZE) == 0 && page_phy_addr >= 0x102000);

  /* user_pool or kernel_pool */
  if (page_phy_addr >= user_pool.phy_addr_start) {
    while (cnt < pg_cnt) {
      page_phy_addr = addr_v2p(vaddr);
      ASSERT((page_phy_addr % PAGE_SIZE) == 0 &&
             page_phy_addr >= user_pool.phy_addr_start);

      pfree(page_phy_addr);
      page_table_pte_remove(vaddr);
      vaddr += PAGE_SIZE;

      cnt++;
    }
  } else {
    while (cnt < pg_cnt) {
      page_phy_addr = addr_v2p(vaddr);
      ASSERT((page_phy_addr % PAGE_SIZE) == 0 &&
             page_phy_addr >= kernel_pool.phy_addr_start &&
             page_phy_addr < user_pool.phy_addr_start);

      pfree(page_phy_addr);
      page_table_pte_remove(vaddr);
      vaddr += PAGE_SIZE;

      cnt++;
    }
  }
  vaddr_remove(pf, _vaddr, pg_cnt);
}

/**
 * sys_free() - Free memory at a given pointer.
 * @ptr: Pointer to the memory to be freed.
 *
 * This function releases the memory allocated at the given pointer. It
 * determines whether the memory belongs to the kernel or user pool and then
 * proceeds to recycle the memory accordingly. It handles both large memory
 * allocations and smaller memory blocks by either freeing the entire arena or
 * recycling individual blocks and potentially the entire arena if all blocks
 * are free.
 *
 * Context: A critical function for memory management, particularly for
 * deallocating dynamically allocated memory. It is the counterpart to memory
 * allocation functions like malloc.
 */
void sys_free(void *ptr) {
  ASSERT(ptr != NULL);
  if (ptr == NULL)
    return;
  enum pool_flags pf;
  struct pool *mem_pool;

  if (running_thread()->pg_dir == NULL) {
    ASSERT((uint32_t)ptr >= KERNEL_HEAP_START);
    pf = PF_KERNEL;
    mem_pool = &kernel_pool;
  } else {
    pf = PF_USER;
    mem_pool = &user_pool;
  }
  lock_acquire(&mem_pool->_lock);

  struct mem_block *b = ptr;
  struct arena *a = block_2_arena(b);
  if (a->desc == NULL && a->large_mb == true) {
    /* large memory blocks larger than 1024 bytes  */
    mfree_page(pf, a, a->cnt);
  } else {
    /* small memory blocks divided within a page  */
    list_append(&a->desc->free_list, &b->free_elem);

    /* the whole page is unused, free it  */
    if (++a->cnt == a->desc->block_per_arena) {
      uint32_t block_idx;
      for (block_idx = 0; block_idx < a->desc->block_per_arena; block_idx++) {
        struct mem_block *b = arena_2_block(a, block_idx);
        ASSERT(list_elem_find(&a->desc->free_list, &b->free_elem));
        list_remove(&b->free_elem);
      }
      mfree_page(pf, a, 1);
    }
  }
  lock_release(&mem_pool->_lock);
}

/**
 * get_page_to_vaddr_without_bitmap() - Allocate a page without operating on
 * the virtual address bitmap.
 * @pf: The pool flag indicating the type of memory pool.
 * @vaddr: The virtual address where the page should be mapped.
 *
 * This function allocates a single page of physical memory and maps it to a
 * given virtual address, without modifying the virtual address bitmap. It's
 * specifically designed for scenarios like fork, where there's no need to alter
 * the virtual address bitmap. It acquires a lock to ensure thread safety,
 * allocates a physical page, adds a page table entry for the mapping, and then
 * releases the lock.
 *
 * Context: Essential in memory management operations where virtual address
 * bitmap operations are not required, such as during process forking.
 *
 * Return: Returns the virtual address to the allocated page, or NULL if
 * allocation fails.
 */
void *get_page_to_vaddr_without_bitmap(enum pool_flags pf, uint32_t vaddr) {
  struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->_lock);

  void *page_phy_addr = palloc(mem_pool);
  if (page_phy_addr == NULL)
    return NULL;
  page_table_add((void *)vaddr, page_phy_addr);
  lock_release(&mem_pool->_lock);
  return (void *)vaddr;
}
