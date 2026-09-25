[bits 64]
global _start

_start:
    ; 1. Afișăm întrebarea
    mov rax, 1                  ; 1 = Syscall Print
    lea rdi, [rel question]
    int 0x80

    ; 2. Citim răspunsul
    mov rax, 2                  ; 2 = Syscall Read Line
    lea rdi, [rel input_buf]    ; Adresa unde se va salva numele
    mov rsi, 50                 ; Citim maxim 50 de caractere
    int 0x80

    ; 3. Afișăm prima parte din răspuns ("Salut, ")
    mov rax, 1                  ; 1 = Syscall Print
    lea rdi, [rel hello_msg]
    int 0x80

    ; 4. Afișăm numele introdus
    mov rax, 1                  ; 1 = Syscall Print
    lea rdi, [rel input_buf]
    int 0x80
    
    ; 5. Trecem pe o linie nouă la final
    mov rax, 1
    lea rdi, [rel newline_str]
    int 0x80

    ret                         ; Ieșim cu bine din program

section .data
question    db "Cum te numesti? ", 0
hello_msg   db "Salutare si bine ai venit in Nano OS, ", 0
newline_str db 10, 0

section .bss
input_buf resb 50               ; Rezervăm un buffer gol de 50 octeți