BITS 64
global isr_syscall
extern syscall_handler

section .text
isr_syscall:
    ; Salvăm registrele la fel ca la tastatură
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

    ; Trimitem pointerul sticlei (registrelor) ca argument în RDI
    mov rdi, rsp 
    call syscall_handler

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

    ; ATENȚIE: Fără EOI (out 0x20, al) aici, pentru că e întrerupere software!
    iretq