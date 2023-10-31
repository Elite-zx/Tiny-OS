SECTION MBR vstart=0x7c00
; initialize register status
mov ax, cs
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov sp, 0x7c00

; clear screen
mov ax, 0x0600
mov bx, 0x0700
mov cx, 0x0
mov dx, 0x184f
int 0x10

; Get cursor position
mov ah, 0x3
mov bh, 0x0
int 0x10

; let es:bp point to message
mov ax, message
mov bp, ax

; size of message
mov cx, 0x0b

; print message
mov ax, 0x1301
mov bx, 0x0002
int 0x10

; infinite loop
jmp $

; define a message,  message below is a label
; 0 is considered the end of the string and can be ignored
message db 'Hello, MBR!'

; fill with zeros until 510 bytes
times 510-($-$$) db 0

; magic number in MBR, which Indicates that this sector has a program that can be loaded
db 0x55, 0xaa 
