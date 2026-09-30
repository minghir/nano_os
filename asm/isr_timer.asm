BITS 64
global isr_timer
extern timer_irq
extern schedule     ; NOUA funcție din C

isr_timer:
    ; 1. Salvăm toate registrele exact cum făceai tu
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

    ; 2. Apelăm funcția ta veche care numără tick-urile (opțional)
    call timer_irq

    ; 3. TRIMITEM EOI către PIC Master (Aici e bine!)
    mov al, 0x20
    out 0x20, al

    ; 4. SCHEDULING! (Aici e magia)
    mov rdi, rsp        ; RDI = argument 1 (current_rsp)
    call schedule       ; Apelăm C-ul! Returnează noul RSP în RAX.
    mov rsp, rax        ; SCHIMBĂM STIVA PE STIVA NOULUI PROCES! 
                        ; (CR3 a fost deja schimbat de C în interiorul funcției)

    ; 5. Recuperăm registrele de pe NOUA stivă
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

    iretq