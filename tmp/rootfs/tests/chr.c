#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("Mod tastatura activ. Apasa taste (tasteaza 'q' pentru iesire):\n");
    
    while (1) {
        char c = nano_read_char();
        
        // Dacă utilizatorul apasă 'q', ieșim din buclă
        if (c == 'q') {
            break;
        }
        char str[2];
        str[0] = c;
        str[1] = '\0';
        nano_print(str);
        // Afișăm tasta apăsată folosind print_int sau un mic syscall de print char
        // Sau o poți trimite direct spre ecran
    }
    
    nano_print("\nIesire din program.\n");
    return 0;
}