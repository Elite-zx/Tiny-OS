/*
 * Author: Xun Morris
 * Time: 2023-11-16
 */

#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "bitmap.h"
#include "stdint.h"

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

#define PG_P_1 1
#define PG_P_0 0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4

extern struct pool kernel_pool, user_pool;
void mem_init();
void *get_kernel_pages(uint32_t pg_cnt);

#endif
