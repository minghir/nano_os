#include "nano_libc.h"

int main(int argc, char* argv[]) {
    // Implicit, dacă nu dăm argumente, listăm directorul curent (ex: ".")
    const char* target_dir = ".";

    // Dacă utilizatorul a oferit un argument (ex: "ls /bin/test/")
    if (argc > 1) {
        target_dir = argv[1];
    }

    // Apelăm funcția din libc trimițându-i calea
    nano_ls(target_dir);
    
    return 0;
}