;------------------------
; Author: Xun Morris |
; Time: 2023-11-01 |
;------------------------

%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
; The stack segment and data segment share the same memory
LOADER_STACK_TOP equ LOADER_BASE_ADDR 
jmp loader_start

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

; Reserve space for other segment descriptors
times 60 dq 0

;------------------------
; set GDTR --> GDT address and size
;------------------------
GDT_SIZE equ $-GDT_BASE
GDT_LIMIT equ GDT_SIZE-1
gdt_ptr: 
dw GDT_LIMIT 
dd GDT_BASE 

;------------------------
; set selector
;------------------------
SELECTOR_CODE  equ ((CODE_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0
SELECTOR_DATA  equ ((DATA_STACK_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0
SELECTOR_DISPLAY  equ ((DISPLAY_DESC-GDT_BASE)/8) << 3 + TI_GDT + RPL0

loadermsg db "2 loader in real mode." 

loader_start:
;------------------------
; print message
;------------------------
mov sp, LOADER_BASE_ADDR
mov bp, loadermsg
mov cx, 22
mov ax, 0x1301
mov bx, 0x001f
mov dx, 0x1800
int 0x10

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

