;------------------------
; Author: Xun Morris |
; Time: 2023-11-21 |
;------------------------

[bits 32]
section .text
global switch_to
switch_to:
push esi
push edi
push ebx
push ebp

; retrieve the PCB address (which is self_kstack) of current thread from parameter 'cur_thread'
; and store esp into this PCB. see P.433 for details
mov eax, [esp+20]
mov [eax], esp

; retrieve the PCB address of next thread from parameter 'next'
mov eax,[esp+24]
mov esp, [eax]

pop ebp
pop ebx
pop edi
pop esi
ret



