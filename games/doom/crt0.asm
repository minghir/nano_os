[bits 64]

section .header
    db 'N', 'A', 'S', '1'    ; Semnătura magică
    dd 8                     ; Offset cod

global _start
extern main

; Keep _start in the header section so the linker cannot insert alignment padding.
_start:
    ; Păstrăm argumentele procesului înainte să folosim RDI pentru syscall.
    mov r12, rdi
    mov r13, rsi

    ; --- PUNEM UN BEEP SAU UN DEBUG DIRECT IN ASM ---
    ; Dacă vrei, putem trimite un caracter direct pe ecran sau un syscall de print (SYSCALL_PRINT = 1)
    mov rdi, msg_start
    mov rax, 1          ; SYSCALL_PRINT
    int 0x80

    ; Restaurăm argc și argv pentru convenția de apel a lui main.
    mov rdi, r12
    mov rsi, r13
    call main           ; Apelăm int main(int argc, char** argv)
    
    mov rdi, rax        
    mov rax, 20         ; SYSCALL_EXIT
    int 0x80            

.hang:
    jmp .hang

section .data
msg_start db "=== CRTO: AM INTRAT IN PROCES! ===", 10, 0