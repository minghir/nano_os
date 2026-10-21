; ===================================================
; Test Program pentru NanoASM v2.0 (Nano OS)
; ===================================================

; 1. Afisare mesaj de start
print "=== Start Test NanoASM v2.0 ===\n"

; 2. Test Aritmetica pe Intregi (ADD / SUB / PRINT_RAX)
mov rax, 10
add rax, 20
sub rax, 5
print "Aritmetica intregi (10 + 20 - 5 = 25): "
print_rax
newline

; 3. Test Inmultire (MUL)
mov rax, 6
mov rdi, 7
mul rdi
print "Inmultire (6 * 7 = 42): "
print_rax
newline

; 4. Test Stiva si Subrutina (PUSH / POP / CALL / RET)
push rax                  ; Salvam 42 pe stiva
call afiseaza_subrutina
pop rax                   ; Restauram 42 din stiva
print "Restaurat RAX din stiva: "
print_rax
newline

; 5. Test Bucla cu salvare in memorie (Evitam coruperea RDI de catre Kernel)
mov rax, 3
mov [contor], rax         ; Initializam contorul in memorie
print "Contorizare bucla (3 2 1): "

bucla:
    ; Afisam valoarea curenta
    mov rax, [contor]
    print_rax
    print " "
    
    ; Scadem contorul in siguranta
    mov rax, [contor]
    sub rax, 1
    mov [contor], rax
    
    ; Facem comparatia (incarcam in RDI pt. ca ASM suporta doar 'cmp rdi, val')
    mov rdi, [contor]
    cmp rdi, 0
    ja bucla              ; Sarim inapoi daca este > 0
    
newline

; 6. Test Operatii Float SSE (MOVSS / ADDSS / PRINT_XMM)
movss xmm0, [val_f1]
movss xmm1, [val_f2]
addss xmm0, xmm1
print "Aritmetica float (3.5 + 2.25 = 5.75): "
print_xmm
newline

; 7. Afisare sir din variabila si Exit
print [msg_final]
exit

; ===================================================
; SUBRUTINE SI DATE
; ===================================================

afiseaza_subrutina:
    print "  -> [Subrutina apelata cu succes!]\n"
    ret

val_f1:
    data 3.5

val_f2:
    data 2.25

msg_final:
    data "=== Toate testele au fost trecute! ===\n"
	
	quad 0
    quad 0
    quad 0
    quad 0 ; 32 de octeți liberi de protecție
	
contor:
    data 0