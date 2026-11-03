#include "nano_libc.h"

int main() {
    nano_print("Testam alocatorul de memorie (malloc)...\n");

    // Alocăm dinamic un șir de caractere
    char* test_str = (char*)nano_malloc(50);
    
    if (test_str == 0) {
        nano_print("Eroare: Malloc a esuat!\n");
        return 1;
    }

    nano_print("Memorie alocata cu succes la adresa dinamica!\n");
    
    // Scriem ceva în memoria alocată dinamic
    test_str[0] = 'O';
    test_str[1] = 'K';
    test_str[2] = '\n';
    test_str[3] = '\0';

    nano_print(test_str);

    return 0;
}