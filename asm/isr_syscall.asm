BITS 64
global isr_syscall
extern syscall_handler

section .text
isr_syscall:
    ; Salvăm registrele generale
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

    ; ========================================================
    ; FIX CRITIC: Alocăm spațiu pentru xmm0 și padding (16+8=24 bytes)
    ; pentru ca stiva să se potrivească PERFECT cu structura "Registers"!
    ; ========================================================
    sub rsp, 24

    ; Păstrăm cadrul și aliniem stiva conform ABI-ului înainte de apelul C.
    mov r12, rsp
    mov rdi, r12
    and rsp, -16
    call syscall_handler
    mov rsp, r12

    ; Refacem stiva (ștergem spațiul alocat pentru xmm0 și padding)
    add rsp, 24
    ; ========================================================

    ; Restaurăm registrele
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

    ; Fără EOI, e întrerupere software!
    iretq