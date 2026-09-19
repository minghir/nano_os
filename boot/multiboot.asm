section .multiboot
align 8
multiboot_header:
    dd 0x1BADB002          ; magic
    dd 0                   ; flags
    dd -(0x1BADB002)       ; checksum

section .text
global _start

_start:
    extern kernel_main
    call kernel_main

.hang:
    jmp .hang

