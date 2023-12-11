;------------------------
; Author: Xun Morris |
; Time: 2023-11-09 |
;------------------------
[bits 32]
section .data
put_int_buffer dq 0

section .text
; ============================================================
; Function put_char: Print a character from the stack at the cursor
; ============================================================
; video selector attribute
TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

global put_char
put_char:
;------------------------
; backup registers
; push all double word register (such as eax,ecx,...)
;------------------------
pushad

;------------------------
; set video memory selector
;------------------------
mov ax, SELECTOR_VIDEO
mov gs, ax

;------------------------
; get the cursor position (16 bits) from the graphics card's register
;------------------------
; high 8 bits
mov dx, 0x03d4
mov al, 0x0e
out dx, al
mov dx, 0x03d5
in al, dx
mov ah, al
; low 8 bits
; specify index value in Address Register
mov dx, 0x03d4
mov al, 0x0f
out dx, al
; retrieve data from Data Register , which represents the register corresponding to the index in Address Register
mov dx, 0x03d5
in al, dx
; Now register bx stores the (linear) coordinates of the cursor
mov bx, ax

;------------------------
; retrieve the character to be printed
;------------------------
; 32 bytes of register space + 4 bytes of caller return address
mov ecx, [esp+36]

;------------------------
; Process the characters to be printed --- do special processing for control characters, and print ordinary visible characters. If the visible characters exceed the screen (cmp bx, 2000), add a carriage return processing operation
;------------------------
cmp cl, 0xd
jz .is_carriage_return
cmp cl, 0xa
jz .is_line_feed
cmp cl, 0x8
jz .is_backspace
jmp .put_other

.is_backspace:
dec bx
; The cursor is the coordinate of the character, and a character occupies 2 bytes, so to write to the video memory through the cursor, the cursor needs x2
; bx: the cursor position ---> the position in the video memory
shl bx, 1

mov byte [gs:bx], 0x20
inc bx
mov byte [gs:bx], 0x07
; restore the bx value (the position of cursor)
; bx: the position in the video memory ---> the cursor position
shr bx, 1
jmp .set_cursor

.put_other:
shl bx, 1
mov [gs:bx], cl
inc bx
mov byte [gs:bx], 0x07
shr bx, 1
inc bx
cmp bx, 2000
jl .set_cursor

; \n --- moves the cursor to the beginning of the next line
.is_line_feed:
; \r --- moves the cursor to the beginning of the same line
; actually I treat \r like \n
.is_carriage_return:
; 16-bit division
xor dx, dx
mov ax, bx
mov si, 80
div si
; let bx (cursor) be the first coordinate of the line (carriage return done!)
sub bx, dx
; let bx (cursor) be the first coordinate of the next line (line feed done!)
add bx, 80
; if the cursor exceeds the screen (the result of instruction jl will be false), scroll the srceen
cmp bx, 2000
jl .set_cursor

;------------------------
; roll_screen for 2 reasons:
; 1.The cursor value exceeds the maximum number of characters on a screen (1999)
; 2.There is a newline character on the last line of the screen
;
; the task that moving the cursor (bx, actually) has been completed in processing the carriage return character
;------------------------
.roll_screen:
; Cover lines 0~23 with lines 1~24
cld
; ((2000-80)*2)/4=960
mov ecx, 960
mov esi, 0xc00b80a0 ; beginning of line 0
mov edi, 0xc00b8000 ; beginning of line 1
; mov 4 bytes (double word) each time
rep movsd

; clear the last line in current screen by filling with whitespace (0x0720)
; 1920*2 = 3840
mov ebx, 3840
mov ecx, 80
.cls:
mov word [gs:ebx], 0x0720
add ebx, 2
loop .cls
; update cursor position info --- beginning of the last line
mov bx, 1920

;------------------------
; Update cursor position info in graphics card
;------------------------
.set_cursor:
; set high 8 bits
mov dx, 0x03d4
mov al, 0x0e
out dx, al

mov dx, 0x03d5
mov al, bh
out dx, al

; set low 8 bits
mov dx, 0x03d4
mov al, 0x0f
out dx, al

mov dx, 0x03d5
mov al, bl
out dx, al

.put_char_end:
popad
ret

; ============================================================
; Function put_str: Print string at the cursor
; ============================================================
global put_str
put_str:
;------------------------
; backup registers
;------------------------
push ebx
push ecx

;------------------------
; , prepare the parameters (the starting address of the string) here
;------------------------
xor ecx, ecx
; 8 bytes of the backup register + 4 bytes of caller return address
mov ebx, [esp+12]

;------------------------
; Implement the function by calling put_char
;------------------------
.impl:
mov cl, [ebx]

; Determine whether the string ends
cmp cl, 0
jz .str_over

push ecx
call put_char
add esp, 4
inc ebx
loop .impl

.str_over:
pop ecx
pop ebx
ret

; ============================================================
; Function put_int: Print integer at the cursor
; Implement: Convert every 4 digits of a 32-bit number (i.e. a hexadecimal number) into corresponding characters
; eg: 0x0012345 --> '0x12345'
; ============================================================
;------------------------
; put_int_buffer serves as a buffer to store the converted results.
;------------------------
global put_int
put_int:
pushad
mov ebp, esp

;jmp $
; the number to be printed stored in register eax
; 32 bytes of register + 4 bytes of caller return address
mov eax, [ebp+36]
mov edx, eax

; edi point to the last byte in buffer
; ecx represents the number of numbers to be processed
mov edi, 7
mov ecx, 8
; ebx represents the base address of the buffer (32bits/4bits = 8)
mov ebx, put_int_buffer

;------------------------
; Convert characters (each 4-bits in 32-bits number) to corresponding ASCII value
;------------------------
.to_char_each_4_bits:
and edx, 0x0000000F
cmp edx, 9
jg .is_A2F
add edx, '0'
jmp .store
.is_A2F:
sub edx, 10
add edx, 'A'

;------------------------
; remove prefix 0
; eg: 0x00000012 ---> 0x12
;------------------------
.store:
mov [ebx+edi], dl
dec edi
shr eax, 4
mov edx, eax
loop .to_char_each_4_bits

.ready_to_print:
; -1 (0xffffffff) +1 = 0
inc edi

; while(num != 0) {}
.skip_prefix_0:
; If 0 (4-bits), then check whether the number to be printed is 0 ( 32-bits )
cmp edi, 8
je .num_is_0

.detect_prefix_0:
mov cl, [put_int_buffer+edi]
inc edi
cmp cl, '0'
je  .skip_prefix_0
dec edi
jmp .put_each_num

.num_is_0:
; the number to be printed is zero
mov cl, '0'

;------------------------
; print characters in buffer
;------------------------
.put_each_num:
push ecx
call put_char
add esp, 4
inc edi
mov cl, [put_int_buffer+edi]
cmp edi, 8
jl .put_each_num
popad
ret

global set_cursor
set_cursor:
pushad
; 32 bytes of 8 registers and 4 bytes of return address
mov bx, [esp+36]
; set high 8 bits
mov dx, 0x03d4
mov al, 0x0e
out dx, al

mov dx, 0x03d5
mov al, bh
out dx, al

; set low 8 bits
mov dx, 0x03d4
mov al, 0x0f
out dx, al

mov dx, 0x03d5
mov al, bl
out dx, al

popad
ret

global sys_clear
sys_clear:
pushad
mov ax, SELECTOR_VIDEO
mov gs, ax

mov ebx, 0
mov ecx, 80*25

.cls:
mov word [gs:ebx],0x0720
add ebx, 2
loop .cls

mov ebx, 0
jmp .set_cursor



