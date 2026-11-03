#include "nano_libc.h"

int main() {
    char nume[50];
    
    nano_print("=== Prima aplicatie C din Nano OS ===\n");
    nano_print("Cum te numesti? ");
    
    nano_readline(nume, 50);
    
    nano_print("Salut, ");
    nano_print(nume);
    nano_print("! Ai scris asta exclusiv in C!\n");
    
    return 0; // Returnează controlul lui crt0.asm, care dă 'ret' în kernel
}