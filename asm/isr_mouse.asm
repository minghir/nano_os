global isr_mouse
extern mouse_handler_main

section .text
isr_mouse:
    cli
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; Aliniere sigură folosind RBP
    push rbp
    mov rbp, rsp
    and rsp, -16
    
    call mouse_handler_main
    
    mov rsp, rbp
    pop rbp

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    ; sti (opțional, iretq restaurează oricum RFLAGS cu interrupt flag-ul corect)
    iretq