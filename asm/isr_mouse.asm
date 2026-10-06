global isr_mouse
extern mouse_handler_main

section .text
isr_mouse:
    cli
    ; Salvăm registrele volatile conform System V AMD64 ABI
    push r12
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov r12, rsp
    and rsp, -16
    call mouse_handler_main
    mov rsp, r12

    ; Restaurăm registrele
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    pop r12
    
    sti
    iretq