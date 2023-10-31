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

; clear screen
mov ax, 0x0600
mov bx, 0x0700
mov cx, 0x0
mov dx, 0x184f
int 0x10

; Write data ("1 MBR") to the graphics card
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

; infinite loop
jmp $

; fill with zeros until 510 bytes
times 510-($-$$) db 0

; magic number in MBR, which Indicates that this sector has a program that can be loaded
db 0x55, 0xaa 
