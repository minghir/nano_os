[bits 64]

section .header
    db 'N', 'A', 'S', '1'    ; Semnătura magică
    dd 8                     ; Offset cod

section .text
global _start
extern main

_start:
    ; --- PUNEM UN BEEP SAU UN DEBUG DIRECT IN ASM ---
    ; Dacă vrei, putem trimite un caracter direct pe ecran sau un syscall de print (SYSCALL_PRINT = 1)
    mov rdi, msg_start
    mov rax, 1          ; SYSCALL_PRINT
    int 0x80

    call main           ; Apelăm int main(int argc, char** argv)
    
    mov rdi, rax        
    mov rax, 20         ; SYSCALL_EXIT
    int 0x80            

.hang:
    jmp .hang

section .data
msg_start db "=== CRTO: AM INTRAT IN PROCES! ===", 10, 0