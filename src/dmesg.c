#include "nano_libc.h"

int main(int argc, char* argv[]) {
    char log_buffer[4096];
    for (int i = 0; i < 4096; i++) log_buffer[i] = '\0';

    nano_getlog(log_buffer, 4095);

    if (log_buffer[0] != '\0') {
        nano_print("--- KERNEL SYSLOG ---\n");
        nano_print(log_buffer);
        nano_print("---------------------\n");
    } else {
        nano_print("Syslog-ul este gol.\n");
    }

    return 0;
}