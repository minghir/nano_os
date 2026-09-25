[bits 64]
global _start

_start:
    mov rax, 1                  ; 1 = Comanda de Print

    ; Folosim LEA cu [rel message] pentru a obține adresa reală din RAM, 
    ; indiferent unde a fost alocat programul de către kernel!
    lea rdi, [rel message]      

    int 0x80                    ; Apelăm întreruperea spre Kernel

    ret                         ; Înapoi în shell

section .data
message db "Hello via Syscall from NANO OS!!!!", 10, 0