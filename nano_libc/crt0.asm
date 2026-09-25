[bits 64]
global _start
extern main

section .text
_start:
    call main       ; Sari în funcția main() din C
    ret             ; Când main() face return 0, ajungem aici și ne întoarcem în Kernel