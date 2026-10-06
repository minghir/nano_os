BITS 64
global isr14
extern page_fault_handler

isr14:
    ; 1. CPU-ul a pus deja Error Code-ul pe stivă, urmat de cadrele IRET (RIP, CS, RFLAGS, RSP, SS).
    ; Salvăm registrele generale în ordinea inversă a push-urilor (exact ca la timer/syscall)
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
    
    ; Aliniere / Salvare XMM0 conform structurii Registers
    sub rsp, 8
    sub rsp, 16
    movdqu [rsp], xmm0

    ; 2. Trimitem pointerul la stivă ca argument (RDI) către funcția C page_fault_handler
    mov r12, rsp
    mov rdi, r12
    and rsp, -16
    call page_fault_handler
    mov rsp, r12

    ; 3. Restaurare registre
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
    
    ; 4. Important: Curățăm Error Code-ul lăsat de procesor pe stivă pentru excepția 14
    add rsp, 8
    
    ; 5. Întoarcere din întrerupere
    iretq