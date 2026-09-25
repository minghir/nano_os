BITS 64
global isr_timer
extern timer_irq

isr_timer:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    sub rsp, 8
    sub rsp, 16
    movdqu [rsp], xmm0

    call timer_irq

    movdqu xmm0, [rsp]
    add rsp, 16
    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Trimite EOI către PIC Master (deoarece IRQ 0 este pe Master)
    mov al, 0x20
    out 0x20, al

    iretq