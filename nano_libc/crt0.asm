[bits 64]

section .header
    db 'N', 'A', 'S', '1'    ; Semnătura magică (4 octeți) header nano
    dd 8                    ; Offset-ul unde începe codul (dimensiunea header-ului = 8 octeți)

section .text
global _start
extern main

_start:
    call main       ; Apelăm funcția main() din C
    ret             ; Ne întoarcem în kernel