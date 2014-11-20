; #########################################################################

      .386
      .model flat, stdcall
      option casemap :none   ; case sensitive

; #########################################################################

      include \masm32\include\windows.inc
      include \masm32\include\user32.inc
      include \masm32\include\kernel32.inc

      includelib \masm32\lib\user32.lib
      includelib \masm32\lib\kernel32.lib

; #########################################################################

    .code


; #########################################################################
simple_if_1 proc

    .if eax == 1
      mov ebx, 1
    .else
      mov ebx, 0
    .endif
    ret
simple_if_1 endp


; #########################################################################
conseq_block_1 proc
    mov eax, 1
    jmp L1
   L1:
    mov eax, 2
    jmp L2
   L2:
    ret
conseq_block_1 endp

start:
    nop
    call simple_if_1
    nop
    call conseq_block_1
    nop
    invoke ExitProcess, 0
end start