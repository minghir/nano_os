#include "nano_libc.h"
void print_2digits(uint8_t n) {
    char buf[3];
    buf[0] = '0' + (n / 10);
    buf[1] = '0' + (n % 10);
    buf[2] = '\0';
    nano_print(buf);
}

int main() {
    DateTime t;
    
    // 1. Cerem timpul de la kernel (Syscall 5)
    get_time(&t);
    
    // 2. Afișăm în format DD/MM/20YY HH:MM:SS
    print_2digits(t.day);
    nano_print("/");
    print_2digits(t.month);
    nano_print("/20"); 
    print_2digits(t.year);
    
    nano_print(" "); // un mic spațiu între dată și oră
    
    print_2digits(t.hour);
    nano_print(":");
    print_2digits(t.minute);
    nano_print(":");
    print_2digits(t.second);
    nano_print("\n"); // un rând nou la final pentru a lăsa shell-ul curat

    // 3. Ieșim din program (returnăm controlul shell-ului)
    return 0;
}