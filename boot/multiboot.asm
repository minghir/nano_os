section .multiboot
align 8
multiboot_header:
    dd 0x1BADB002          ; magic
    dd 0                   ; flags
    dd -(0x1BADB002)       ; checksum

section .text
global _start

_start:
    mov esp, stack_top
    and esp, 0xFFFFFFF0
    cld
    extern kernel_main
    call kernel_main

.hang:
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

