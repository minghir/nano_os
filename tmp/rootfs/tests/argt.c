#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("Argumente primite:\n");
    
    for (int i = 0; i < argc; i++) {
        nano_print("argv[");
        // poți afișa indexul sau direct argumentul
        nano_print(argv[i]);
        nano_print("]\n");
    }
    
    return 0;
}