[bits 64]
global _start

_start:
    ; Începem fix de la marginea din stânga (Coloana 0, Linia 5)
    mov rdi, 0xB8320        

    mov word [rdi + 0],  0x0A48   ; H
    mov word [rdi + 2],  0x0A65   ; e
    mov word [rdi + 4],  0x0A6C   ; l
    mov word [rdi + 6],  0x0A6C   ; l
    mov word [rdi + 8],  0x0A6F   ; o
    mov word [rdi + 10], 0x0A20   ; [spațiu]
    mov word [rdi + 12], 0x0A66   ; f
    mov word [rdi + 14], 0x0A72   ; r
    mov word [rdi + 16], 0x0A6F   ; o
    mov word [rdi + 18], 0x0A6D   ; m
    mov word [rdi + 20], 0x0A20   ; [spațiu]
    mov word [rdi + 22], 0x0A4E   ; N
    mov word [rdi + 24], 0x0A41   ; A
    mov word [rdi + 26], 0x0A4E   ; N
    mov word [rdi + 28], 0x0A4F   ; O
    mov word [rdi + 30], 0x0A21   ; !

    ret                           ; Înapoi în kernel