BITS 64
global isr_timer
extern timer_irq
extern schedule    ; <--- Adăugăm funcția noastră C

section .text
isr_timer:
    ; 1. Salvăm contextul complet pe stiva procesului curent
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

    ; 2. Rulăm timer_irq ca de obicei (pentru a actualiza system_ticks, ceasul, etc.)
    call timer_irq

    ; 3. CONTEXT SWITCH: Apelăm scheduler-ul
    ; Punem RSP-ul curent în RDI (primul argument pentru funcția C)
    mov rdi, rsp
    call schedule
    
    ; RAX conține acum noul RSP (returnat de schedule). 
    ; Dacă e același proces, va fi aceeași valoare. Dacă e un proces nou, schimbăm stiva!
    mov rsp, rax

    ; 4. Trimite EOI către PIC Master
    mov al, 0x20
    out 0x20, al

    ; 5. Restaurăm contextul (de pe NOUA stivă sau VECHEA stivă)
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