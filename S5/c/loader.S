;------------------------
; Author: Xun Morris |
; Time: 2023-11-05 |
;------------------------

%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
; The stack segment and data segment share the same memory
LOADER_STACK_TOP equ LOADER_BASE_ADDR 

; ============================================================
; build GDT by filling entries (in data segment)
; ============================================================

GDT_BASE:
dd 0x00000000
dd 0x00000000

CODE_DESC:
dd 0x0000FFFF
dd DESC_CODE_HIGH4

DATA_STACK_DESC:
dd 0x0000FFFF
dd DESC_DATA_HIGH4

DISPLAY_DESC:
dd 0x80000007
dd DESC_DISPLAY_HIGH4

; Macro in assembly do not take up memory space
; get size of gdt,
GDT_SIZE equ $-GDT_BASE 

;------------------------
; set selector
;------------------------
SELECTOR_CODE  equ ((CODE_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0
SELECTOR_DATA  equ ((DATA_STACK_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0
SELECTOR_DISPLAY  equ ((DISPLAY_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0

; Reserve space for other segment descriptors
times 60 dq 0

;------------------------
; define variable: store memory size information
;------------------------
; so far, defined 64*8 = 512 (0x200) bytes, which is 64 descriptors.
; so the address of total_mem_bytes (stand for total memory size) is 0x0900+0x0200=0x0b00
total_mem_bytes:
dd 0

;------------------------
; set GDTR --> GDT address and size
;------------------------
GDT_LIMIT equ GDT_SIZE-1
gdt_ptr: 
dw GDT_LIMIT 
dd GDT_BASE 

;------------------------
; define ARDS: Address Range Descriptor Structure
;------------------------
; The ARDS buffer and ARDS number size below is just to align it exactly to offset 0x200+6B+244B+2B = 0x300
ARDS_buf:
times 244 db 0
ARDS_num:
dw 0

; LOADER_BASE_ADDR + 0x300
loader_start:
;------------------------
; Subfunction 0xE820 of BIOS 0x15 interrupt 
;------------------------
xor ebx, ebx
mov edx, 0x534d4150
mov di, ARDS_buf

.E820_mem_retrieve_loop:
mov eax, 0x0000e820
mov ecx, 20
int 0x15

; jump if carry (bit CF is set to 1)
jc .E820_failed_so_try_E801

add di, cx
inc word [ARDS_num]
cmp ebx, 0 
jnz .E820_mem_retrieve_loop

; Traverse the ARDS structure to find the largest memory area
mov cx, [ARDS_num]
mov ebx, ARDS_buf
; Use edx to store the maximum capacity currently traversed
xor edx, edx

.find_max_mem_area:
mov eax, [ebx]
add eax, [ebx+8]
add ebx, 20
cmp edx, eax
jge .next_ards
mov edx, eax

.next_ards:
loop .find_max_mem_area
jmp .mem_retrieve_ok

;------------------------
; Subfunction 0xE801 of BIOS 0x15 interrupt 
;------------------------
.E820_failed_so_try_E801:
mov ax, 0xe801
int 0x15
jc .E801_failed_so_try_88

; calculate the memory below 15MB
mov cx,0x400
; 16-bit multiplication, the high 16 bits are in dx, the low 16 bits are in ax
mul cx

shl edx, 16
and eax, 0x0000FFFF
or edx, eax
add edx, 0x100000
mov esi, edx

; calculate the memory between 16MB and 4GB
xor eax, eax
mov ax, bx
mov ecx, 0x10000
; 32-bit multiplication, the high 32 bits are in edx (zero for 4GB), the low 32 bits are in eax
mul ecx

add esi, eax
mov edx, esi
jmp .mem_retrieve_ok

;------------------------
; Subfunction 0x88 of BIOS 0x15 interrupt 
;------------------------
.E801_failed_so_try_88:
mov ah, 0x88
int 0x15
jc .error_hlt

; keep ax, which stand for the amount of memory (in 1kb units) that stores more than 1MB
and eax, 0x0000FFFF

mov cx, 0x400
mul cx
shl edx, 16
or edx, eax
add edx, 0x100000

; halt CPU until the next external interrupt is fired (happened)
.error_hlt:
hlt 

.mem_retrieve_ok:
mov [total_mem_bytes], edx

;------------------------
; Start entering protected mode in three steps
;------------------------

; 1. turn on A20
in al, 0x92
or al, 0000_0010b
out 0x92, al

; 2. load GDT
lgdt [gdt_ptr]

; 3. set bit PE of CRO to 1
mov eax, cr0
or eax, 0x00000001
mov cr0, eax

; refresh CPU pipeline-> 16bits to 32bits
jmp dword SELECTOR_CODE:real_mode_start


;------------------------
; Now CPU enter real mode,
; I (The OS maker --- Mr.Morris) am about to start making great achievements.
;------------------------
[bits 32]
real_mode_start:
mov ax, SELECTOR_DATA
mov ds, ax
mov es, ax

mov ss, ax
mov esp, LOADER_STACK_TOP

mov ax, SELECTOR_DISPLAY
mov gs, ax


;------------------------
; Load kernel into memory
;------------------------
mov eax, KERNEL_START_SECTOR
mov ebx, KERNEL_BIN_BASE_ADDR
mov ecx, 200
call rd_disk_m_16


;------------------------
; Start page mode in three steps
;------------------------
; 1. Get PDT and PT ready
call setup_page

; Modify the segment base address in the segment descriptor of the video(display) memory segment (now a virtual address)
; sgdt puts the contents of GDTR into the memory at the specified address
sgdt [gdt_ptr]
; gdt_ptr + 2 is the address of GDT 
mov ebx, [gdt_ptr+2]
; ebx+0x18+4 is the upper 4 bytes of the third segment descriptor (video memory segment descriptor)
; The OR operation with 0xc0000000 is to modify the highest byte of this segment descriptor and map the video memory segment to the upper 1GB (which is kernel space) of 4GB.
or dword [ebx+0x18+4], 0xc0000000

; Modify the base address of GDT itself
add dword [gdt_ptr+2], 0xc0000000
add esp, 0xc0000000

; 2. put the address of PDT into CR3
mov eax, PAGE_DIR_TABLE_POS
mov cr3, eax

; 3. turn on bit pg (31) on cr0
mov eax, cr0
or eax, 0x80000000
mov cr0, eax

; update the value of GDTR
lgdt [gdt_ptr]

mov byte [gs:160], 'V'

jmp $

; ============================================================
; Function: Create page directory table and page table
; ============================================================
; Two page directory entries (1, 768) are created here

; Clear the 4KB bytes occupied by the page directory byte by byte
setup_page:
mov ecx, 4096
mov esi, 0
.clear_PDT:
mov byte [PAGE_DIR_TABLE_POS+esi], 0
inc esi
loop .clear_PDT

; Start creating page directory entries
.create_PDE:
mov eax, PAGE_DIR_TABLE_POS
; The PDT starts from 0x10000, which itself occupies 0x1000. So the first page table is at address 0x101000
add eax, 0x1000 
mov ebx, eax
or eax, PG_US_U | PG_RW_W | PG_P
; create the first PDE
mov [PAGE_DIR_TABLE_POS + 0x0], eax
; create the 768th PDE -> The purpose is to map the virtual address 3GB (0xc0000000)~3GB+4MB (0xc03fffff) to the first page table, and then map it to the physical address 0~4MB, that is, the first standard page
mov [PAGE_DIR_TABLE_POS+ 0xc00], eax

; Let the last page directory entry store the starting address of the PDT
sub eax, 0x1000
mov [PAGE_DIR_TABLE_POS+4092], eax

;------------------------
; Create page table entry
;------------------------
; A complete page table corresponds to 4MB of physical memory, but Xun-Morris-Tiny-OS kernel only requires 1MB (256 *4KB) of space. So only 256 page table entries are actually created first
mov ecx, 256
mov esi, 0
mov edx, PG_US_U | PG_RW_W | PG_P
.create_PTE:
mov [ebx+esi*4], edx
; One page table entry corresponds to 4KB of physical memory, so add 4096
add edx, 4096
inc esi
loop .create_PTE

;------------------------
; Create page directory entry for OS kernel
; PDE 769~1022 are created here
;------------------------
; Map the upper 1GB virtual memory (3GB~4GB) where the operating system kernel is located to the physical memory 0~1GB
mov eax, PAGE_DIR_TABLE_POS
; eax stand for the address of page table
add eax, 0x2000
or eax, PG_US_U | PG_RW_W | PG_P
mov ebx, PAGE_DIR_TABLE_POS
; 254 PDEs
mov ecx, 254
mov esi, 769
.create_kernel_PDE:
mov [ebx+esi*4], eax
inc esi
; the size of a PT is 4KB(0x1000)
add eax, 0x1000
loop .create_kernel_PDE
ret

; ============================================================
; Function: read n sectors from disk
; ============================================================
rd_disk_m_32:
;---------------------------------------------
; set sector count
;---------------------------------------------
mov esi, eax
mov dx, 0x1f2
; 8 bits for this I/O ports (all 8 bits except data)
mov al, cl
out dx, al
mov eax, esi

;---------------------------------------------
; set LBA low
;---------------------------------------------
mov dx, 0x1f3
out dx, al

;---------------------------------------------
; set LBA mid
;---------------------------------------------
push cx
mov cl, 8
shr eax, cl
mov dx, 0x1f4
out dx, al

;---------------------------------------------
; set LBA high
;---------------------------------------------
shr eax, cl
mov dx, 0x1f5
out dx, al

;---------------------------------------------
; set device
;---------------------------------------------
shr eax, cl
; keep last 4 bits: 24~27 in LBA
and al, 0x0f
; enable LBA address mode, 0xe0->0x1110, 0000
or al, 0xe0
mov dx, 0x1f6
out dx, al

;---------------------------------------------
; set command: read disk
;---------------------------------------------
mov dx, 0x1f7
mov al, 0x20
out dx, al

;---------------------------------------------
; check disk status
;---------------------------------------------
.not_ready:
nop
; read from the same port: 0x1f7 -- Status reg 
in al, dx
; check the third bit in Status reg
and al, 0x88
cmp al, 0x08
jnz .not_ready

;---------------------------------------------
; read disk
;---------------------------------------------
; cx -> ax
pop ax
mov dx, 256
mul dx
mov cx, ax

mov dx, 0x1f0
.go_on_read:
in ax, dx
mov [ebx], ax
add ebx, 2
loop .go_on_read
ret 

; ============================================================
; Parse kernel ELF file 
; ============================================================
kernel_init:
xor eax, eax
xor ebx, ebx
xor ecx, ecx
xor edx, edx

; extract program header info from ELF file header
; e_phentsize (2 bytes) -> size of program header table 
mov dx, [KERNEL_BIN_BASE_ADDR + 42]
; e_phoff (4 bytes) -> start of program header table
mov ebx, [KERNEL_BIN_BASE_ADDR + 28]
add ebx,  KERNEL_BIN_BASE_ADDR
; e_phnum (2 bytes) -> entry count of program header table
mov cx, [KERNEL_BIN_BASE_ADDR++44]


