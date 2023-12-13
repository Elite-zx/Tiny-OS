/*
 * Author: Zhang Xun
 * Time: 2023-11-29
 */

#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "bitmap.h"
#include "list.h"
#include "stdint.h"

#define PG_P_1 1
#define PG_P_0 0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4

#define MB_DESC_CNT 7

/**
 * struct virtual_addr - Manages a virtual memory pool.
 * @vaddr_bitmap: Bitmap for tracking the allocation status of virtual
 * addresses.
 * @vaddr_start: The starting virtual address of the memory pool.
 *
 * This structure is used to manage a virtual memory pool, which is crucial for
 * virtual memory management in an operating system. It includes a bitmap that
 * keeps track of allocated and free virtual addresses within the pool. The
 * 'vaddr_start' member denotes the beginning of the virtual address space
 * managed by this pool. This structure helps in allocating virtual addresses
 * dynamically and efficiently, ensuring proper management of virtual memory
 * space.
 */
struct virtual_addr {
  struct bitmap vaddr_bitmap;
  uint32_t vaddr_start;
};

enum pool_flags { PF_KERNEL = 1, PF_USER = 2 };

/**
 * mem_block - memory block struct
 * @free_elem: element in free list
 */
struct mem_block {
  struct list_elem free_elem;
};

/**
 * struct mem_block_desc - Memory Block Descriptor.
 * @block_size: Size of each memory block.
 * @blocks_per_arena: Number of blocks that this arena can hold.
 * @free_list: List of currently available memory blocks.
 *
 * This structure is used to describe properties of memory blocks, including
 * their size, the number of blocks per arena, and a list of free blocks.
 */
struct mem_block_desc {
  uint32_t block_size;
  uint32_t block_per_arena;
  struct list free_list;
};

extern struct pool kernel_pool, user_pool;
void mem_init();
void *get_kernel_pages(uint32_t pg_cnt);
void *get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc *k_mb_desc_arr);
void *sys_malloc(uint32_t _size);
void sys_free(void *ptr);
void *get_page_to_vaddr_without_bitmap(enum pool_flags pf, uint32_t vaddr);
void mfree_page(enum pool_flags pf, void *_vaddr, uint32_t pg_cnt);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);
#endif
