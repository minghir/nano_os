#include "nano_libc.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    char path_buffer[128];
    nano_pwd(path_buffer, sizeof(path_buffer));

    nano_print(path_buffer);
    newline();
    
    return 0;
}