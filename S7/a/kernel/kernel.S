;------------------------
; Author: Xun Morris |
; Time: 2023-11-11 |
;------------------------

; ============================================================
; Define interrupt handler
; ============================================================
[bits 32]
;------------------------
; Pushing 4 bytes of 0 when there is no ERROR CODE for interrupt.
; The intention is to unfiy stack operation.
; When cpu execute iret, ESP must point to EIP.
;------------------------
%define ERROR_CODE nop
%define ZERO push 0

extern put_str

section .data
intr_str db "interrupt occur!", 0xa, 0

;------------------------
; Address array for interrupt handler
;------------------------
global intr_entry_table
intr_entry_table:

;------------------------
; A function defined as a macro that accepts 2 parameters.
; Function: print string on screen.
;------------------------
%macro VECTOR 2
section .text
intr_%1_entry:
; here maybe push 0 or nop. This instuctor must be executed first
; see P.323 for details
%2
push intr_str
call put_str
add esp, 4

; Set the OCW2 (operation command) register to send the EOI signal.
; by setting bit EOI to 1 (00100000b -> 0x20).
mov al, 0x20
out 0xa0, al
out 0x20, al

; let esp cross ERROR_CODE on the stack
add esp, 4
iret

; store the entry address of the interrupt handler
section .data
dd intr_%1_entry

%endmacro

;------------------------
; Call macro function, pass two arguments.
; If the interrupt event does not push ERROR CODE onto the stack, pass the argument zero; which represent push 4 bytes of 0.
;
; The lowest usable interrupt vector number for OS kernel is 32 (numbered starting from ;0), so a total of 33 interrupts is defined here
;------------------------
VECTOR 0X00, ZERO
VECTOR 0X01, ZERO
VECTOR 0X02, ZERO
VECTOR 0X03, ZERO
VECTOR 0X04, ZERO
VECTOR 0X05, ZERO
VECTOR 0X06, ZERO
VECTOR 0X07, ZERO
VECTOR 0X08, ERROR_CODE
VECTOR 0X09, ZERO
VECTOR 0X0a, ERROR_CODE
VECTOR 0X0b, ERROR_CODE
VECTOR 0X0c, ERROR_CODE
VECTOR 0X0d, ERROR_CODE
VECTOR 0X0e, ERROR_CODE
VECTOR 0X0f, ZERO
VECTOR 0X10, ZERO
VECTOR 0X11, ERROR_CODE
VECTOR 0X12, ZERO
VECTOR 0X13, ZERO
VECTOR 0X14, ZERO
VECTOR 0X15, ZERO
VECTOR 0X16, ZERO
VECTOR 0X17, ZERO
VECTOR 0X18, ZERO
VECTOR 0X19, ZERO
VECTOR 0X1a, ZERO
VECTOR 0X1b, ZERO
VECTOR 0X1c, ZERO
VECTOR 0X1d, ZERO
VECTOR 0X1e, ERROR_CODE
VECTOR 0X1f, ZERO
VECTOR 0X20, ZERO
