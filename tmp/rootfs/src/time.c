#include "nano_libc.h"

// Funcție ajutătoare în programul tău pentru afișare frumoasă (ex: "09" în loc de "9")
void print_2digits(uint8_t n) {
    char buf[3];
    buf[0] = '0' + (n / 10);
    buf[1] = '0' + (n % 10);
    buf[2] = '\0';
    nano_print(buf);
}

int main() {
    DateTime t;
    
    // Apelăm noul syscall!
    get_time(&t);
    
    //nano_print("Timpul sistemului este: ");
    print_2digits(t.hour);
    nano_print(":");
    print_2digits(t.minute);
    nano_print(":");
    print_2digits(t.second);
    nano_print("\n");
    
    return 0;
}