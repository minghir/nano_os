[bits 64]
global _start
extern main

section .text
_start:
    call main       ; Apelăm funcția main() din C
    ret             ; Ne întoarcem elegant în kernel (sau în handler-ul de syscall)