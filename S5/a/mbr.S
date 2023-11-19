;------------------------
; Author: Xun Morris |
; Time: 2023-11-02 |
;------------------------
%include "boot.inc"
SECTION MBR vstart=0x7c00
; initialize register status
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov sp, 0x7c00
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
; Write data ("1 MBR") to the graphics card
;---------------------------------------------
mov byte [gs:0x00], '1'
; 0x1010 0100 -> foreground is red, background is green, blink 
mov byte [gs:0x01], 0xA4 


mov byte [gs:0x02], ' '
mov byte [gs:0x03], 0xA4

mov byte [gs:0x04], 'M'
mov byte [gs:0x05], 0xA4


mov byte [gs:0x06], 'B'
mov byte [gs:0x07], 0xA4


mov byte [gs:0x08], 'R'
mov byte [gs:0x09], 0xA4

; macro defined in boot inc
mov eax, LOADER_START_SECTOR
mov bx, LOADER_BASE_ADDR
mov cx, 4
call rd_disk_m_16
jmp LOADER_BASE_ADDR + 0x300
;jmp $

; ============================================================
; function: read n sectors from disk
; ============================================================
rd_disk_m_16:
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
pop ax
mov dx, 256
mul dx
mov cx, ax

mov dx, 0x1f0
.go_on_read:
in ax, dx
mov [bx], ax
add bx, 2
loop .go_on_read
ret 

; fill with zeros until 510 bytes
times 510-($-$$) db 0

; magic number in MBR, which Indicates that this sector has a program that can be loaded
db 0x55, 0xaa 
