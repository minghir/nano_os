#include "nano_libc.h"

int main(int argc, char** argv) {
    // Apelăm noul nostru syscall prin wrapper-ul din libc
    uint32_t uid = nano_getuid();
    
    // Traducem UID-ul numeric în nume (exact ca la ps)
    if (uid == 0) {
        nano_print("root\n");
    } else if (uid == 1000) {
        nano_print("user\n");
    } else {
        nano_print("unknown\n");
    }
    
    nano_sys_exit(0);
}