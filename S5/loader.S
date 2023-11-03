;------------------------
; Author: Xun Morris |
; Time: 2023-11-02 |
;------------------------

%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
; The stack segment and data segment share the same memory
LOADER_STACK_TOP equ LOADER_BASE_ADDR 

;------------------------
; build GDT by filling entries (in data segment)
;------------------------
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

.mem_retrieve_ok:
mov [total_mem_bytes], edx

; halt CPU until the next external interrupt is fired (happened)
.error_hlt:
hlt 

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

[bits 32]
real_mode_start:
mov ax, SELECTOR_DATA
mov ds, ax
mov es, ax

mov ss, ax
mov esp, LOADER_STACK_TOP

mov ax, SELECTOR_DISPLAY
mov gs, ax

mov byte [gs:160], 'R'

jmp $


