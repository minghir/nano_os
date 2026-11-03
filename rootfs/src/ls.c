#include "nano_libc.h"

int main(int argc, char* argv[]) {
    const char* target_dir = ".";
    if (argc > 1) {
        target_dir = argv[1];
    }

    nano_ls(target_dir);
    
    // Încheiem execuția prin sistemul oficial de syscall-uri
    
    return 0;
}