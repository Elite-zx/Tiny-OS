/*
 * Author: Zhang Xun
 * Time: 2023-12-13
 */
#include "fs.h"
#include "global.h"
#include "list.h"
#include "memory.h"
#include "stdint.h"
#include "stdio.h"
#include "stdio_kernel.h"
#include "string.h"
#include "thread.h"

#define EI_NIDENT (16)
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Addr;

extern void intr_exit();

/* The ELF file header.  This appears at the start of every ELF file.  */
struct Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  Elf32_Half e_type;                /* Object file type */
  Elf32_Half e_machine;             /* Architecture */
  Elf32_Word e_version;             /* Object file version */
  Elf32_Addr e_entry;               /* Entry point virtual address */
  Elf32_Off e_phoff;                /* Program header table file offset */
  Elf32_Off e_shoff;                /* Section header table file offset */
  Elf32_Word e_flags;               /* Processor-specific flags */
  Elf32_Half e_ehsize;              /* ELF header size in bytes */
  Elf32_Half e_phentsize;           /* Program header table entry size */
  Elf32_Half e_phnum;               /* Program header table entry count */
  Elf32_Half e_shentsize;           /* Section header table entry size */
  Elf32_Half e_shnum;               /* Section header table entry count */
  Elf32_Half e_shstrndx;            /* Section header string table index */
};

/* Program segment header.  */
struct Elf32_Phdr {
  Elf32_Word p_type;   /* Segment type */
  Elf32_Off p_offset;  /* Segment file offset */
  Elf32_Addr p_vaddr;  /* Segment virtual address */
  Elf32_Addr p_paddr;  /* Segment physical address */
  Elf32_Word p_filesz; /* Segment size in file */
  Elf32_Word p_memsz;  /* Segment size in memory */
  Elf32_Word p_flags;  /* Segment flags */
  Elf32_Word p_align;  /* Segment alignment */
};

enum segment_type {
  PT_NULL,    /* Program header table entry unused */
  PT_LOAD,    /* Loadable program segment */
  PT_DYNAMIC, /* Dynamic linking information */
  PT_INTERP,  /* Program interpreter */
  PT_NOTE,    /* Auxiliary information */
  PT_SHLIB,   /* Reserved */
  PT_PHDR     /* Entry for header table itself */
};

static bool segment_load(int32_t fd, uint32_t offset, uint32_t file_sz,
                         uint32_t vaddr) {

  /******** caculate the number of pages required for segment ********/
  /* the first page of segment */
  uint32_t vaddr_first_page = vaddr & 0xfffff000;
  /* the remaining size of the first page starting from the segment */
  uint32_t size_in_first_page = PAGE_SIZE - (vaddr & 0x00000fff);

  /* total number of pages required for segment */
  uint32_t segment_page_count = 0;

  if (file_sz > size_in_first_page) {
    uint32_t left_size = file_sz - size_in_first_page;
    /* '+1' here means the first page  */
    segment_page_count = DIV_ROUND_UP(left_size, PAGE_SIZE) + 1;
  } else {
    /* all segment are in the first page  */
    segment_page_count = 1;
  }
  /******** allocate memory space for segment ********/
  /* The page table used is the page table of the old process   */
  uint32_t page_idx = 0;
  uint32_t vaddr_page = vaddr_first_page;
  while (page_idx < segment_page_count) {
    uint32_t *pde = pde_ptr(vaddr_page);
    uint32_t *pte = pte_ptr(vaddr_page);
    /* the corresponding physical page frame doesn't exist   */
    if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
      printf("\nallocate!\n");
      /* allocate a physical page frame */
      if (get_a_page(PF_USER, vaddr_page) == NULL) {
        printf("error!\n");
        return false;
      }
    }
    vaddr_page += PAGE_SIZE;
    page_idx++;
  }
  /******** load segment into memory ********/
  sys_lseek(fd, offset, SEEK_SET);
  printf("\nsegment_load success\n");
  sys_read(fd, (void *)vaddr, file_sz);
  printf("test here\n");
  return true;
}

static int32_t load(const char *pathname) {
  struct Elf32_Ehdr elf_header;
  struct Elf32_Phdr prog_header;
  memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));

  int32_t fd = sys_open(pathname, O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  int32_t ret_val = -1;
  if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) !=
      sizeof(struct Elf32_Ehdr)) {
    ret_val = -1;
    goto done;
  }

  /******** check if it is ELF format ********/
  if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) ||
      elf_header.e_type != 2 || elf_header.e_machine != 3 ||
      elf_header.e_version != 1 || elf_header.e_phnum > 1024 ||
      elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
    ret_val = -1;
    goto done;
  }

  Elf32_Off prog_header_offset = elf_header.e_phoff;
  Elf32_Half prog_header_entry_size = elf_header.e_phentsize;
  Elf32_Half prog_header_entry_count = elf_header.e_phnum;

  printf("hello5");
  uint32_t prog_idx = 0;
  while (prog_idx < prog_header_entry_count) {
    memset(&prog_header, 0, prog_header_entry_size);
    sys_lseek(fd, prog_header_offset, SEEK_SET);

    /* get program header table entry, which contain the information of segment
     */
    if (sys_read(fd, &prog_header, prog_header_entry_size) !=
        prog_header_entry_size) {
      ret_val = -1;
      goto done;
    }
    printf("hello6");

    if (prog_header.p_type == PT_LOAD) {
      if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz,
                        prog_header.p_vaddr)) {
        printf("\nsegment_load failed\n");
        ret_val = -1;
        goto done;
      }
    }
    printf("hello7");
    /* next program header entry (alse means next segment ^_^)  */
    prog_header_offset += prog_header_entry_size;
    prog_idx++;
  }
  ret_val = elf_header.e_entry;
done:
  sys_close(fd);
  return ret_val;
}

int32_t sys_execv(const char *path, char *const argv[]) {
  /******** get the number of arguments ********/
  int32_t argc = 0;
  while (argv[argc]) {
    argc++;
  }
  printk("hello1\n");

  /******** load process from file system ********/
  int32_t entry_point = load(path);
  if (entry_point == -1)
    return -1;

  printk("hello2\n");
  /******** change the name of current process to the name of process just
   * loaded ********/
  struct task_struct *cur = running_thread();
  memcpy(cur->name, path, TASK_NAME_LEN);
  cur->name[TASK_NAME_LEN - 1] = 0;

  /******** preparation before executing new process ********/
  struct intr_stack *intr_stack_0 =
      (struct intr_stack *)((uint32_t)cur + PAGE_SIZE -
                            sizeof(struct intr_stack));
  intr_stack_0->ebx = (int32_t)argv;
  intr_stack_0->ecx = argc;
  intr_stack_0->eip = (void *)entry_point;
  /* reset stack pointer to the bottom of stack */
  intr_stack_0->esp = (void *)0xc0000000;

  /******** starting execute new process by pretending to return from interrupt
   * ********/
  printk("hello3\n");
  asm volatile("movl %0, %%esp; jmp intr_exit" ::"g"(intr_stack_0) : "memory");

  /* make gcc happy  */
  return 0;
}
