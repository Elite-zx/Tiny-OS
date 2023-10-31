;------------------------
; Author: Xun Morris |
; Time: 2023-10-31 |
;------------------------

%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov sp, LOADER_BASE_ADDR
; Starting address of graphics card text mode
mov ax, 0xb800
mov gs, ax

;---------------------------------------------
; clear screen
;---------------------------------------------
mov ax, 0x0600
mov bx, 0x0700
mov cx, 0x0
mov dx, 0x184f
int 0x10

;---------------------------------------------
; Write data ("2 loader") to the graphics card
;---------------------------------------------
mov byte [gs:0x00], '2'
mov byte [gs:0x01], 0xA4 

mov byte [gs:0x02], ' '
mov byte [gs:0x03], 0xA4

mov byte [gs:0x04], 'l'
mov byte [gs:0x05], 0xA4

mov byte [gs:0x06], 'o'
mov byte [gs:0x07], 0xA4

mov byte [gs:0x08], 'a'
mov byte [gs:0x09], 0xA4

mov byte [gs:0x0a], 'd'
mov byte [gs:0x0b], 0xA4

mov byte [gs:0x0c], 'e'
mov byte [gs:0x0d], 0xA4

mov byte [gs:0x0e], 'r'
mov byte [gs:0x0f], 0xA4

jmp $
