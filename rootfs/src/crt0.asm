[bits 64]

section .header
    db 'N', 'A', 'S', '1'    ; Semnătura magică
    dd 8                     ; Offset cod

section .text
global _start
extern main

_start:
    call main       ; Apelăm int main(int argc, char** argv)
    
    mov rdi, rax    ; Punem valoarea returnată de main() în RDI (ca argument pentru exit)
    mov rax, 20     ; Apelăm SYSCALL_EXIT (presupunem că e 20)
    int 0x80        ; TRAGEM INTRERUPEREA CĂTRE KERNEL!

    ; Kernel-ul va opri procesul la int 0x80. 
    ; Dar în caz că ceva eșuează și se întoarce aici, 
    ; facem o buclă infinită ca să nu dea crash prin executarea de memorie aleatoare:
.hang:
    jmp .hang