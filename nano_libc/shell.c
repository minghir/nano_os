#include "nano_libc.h"

int main() {
    nano_print("\nNano OS User-Space Shell v1.0\n");
    char command_buffer[128];

    while (1) {
        // Afișăm promptul clar pe un rând nou
        nano_print("\nnano:/# ");
        
        // Resetăm bufferul
        for (int i = 0; i < 128; i++) {
            command_buffer[i] = '\0';
        }
        
        nano_readline(command_buffer, 127);

        // Curățăm caracterele newline sau carriage return de la sfârșit
        for (int i = 0; i < 128; i++) {
            if (command_buffer[i] == '\n' || command_buffer[i] == '\r' || command_buffer[i] == '\0') {
                command_buffer[i] = '\0';
                break;
            }
        }

        // Dacă s-a tastat gol, reluăm
        if (command_buffer[0] == '\0') {
            continue;
        }

        // Comenzi interne ale shell-ului
        if (strcmp(command_buffer, "exit") == 0) {
            nano_print("La revedere!\n");
            break;
        } 
        else if (strcmp(command_buffer, "about") == 0) {
            nano_print("Nano OS: educational x86-64 kernel");
        } 
        else if (strcmp(command_buffer, "help") == 0) {
            nano_print("Comenzi interne: help, exit\n");
            nano_print("Programe externe: date, time, mandel\n");
        }
        
        else if (starts_with(command_buffer, "cd ")) {
            const char* path = command_buffer + 3;

            if (nano_cd(path)) {
                nano_print("Director schimbat.\n");
            } else {
                nano_print("Eroare: director inexistent.\n");
            }
        }
        
        else {
            // Trecem la rând nou înainte de rularea programului extern
            nano_print("\n");
            
            int success = nano_exec(command_buffer);
            if (!success) {
                nano_print("Comanda necunoscuta: ");
                nano_print(command_buffer);
                nano_print("\n");
            }
        }
    }

    return 0;
}