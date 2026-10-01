#include "nano_libc.h"

int main() {
    nano_print("\nNano OS User-Space Shell v1.0\n");
    char command_buffer[128];

    while (1) {
        // Afișăm promptul clar pe un rând nou
        char cwd[64];
        for(int i=0; i<64; i++) cwd[i] = '\0';
        
        if (nano_getcwd(cwd, 63) <= 0 || cwd[0] == '\0') {
            string_copy(cwd, "/"); // Fallback dacă nu e setat
        }

        // Afișăm promptul dinamic
        nano_print("nano:");
        nano_print(cwd);
        nano_print("# ");
        
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
		if (strcmp(command_buffer, "clear") == 0) {
            nano_clear_screen();
        } 
        else if (strcmp(command_buffer, "about") == 0) {
            nano_print("Nano OS: educational x86-64 kernel");
        } 
        else if (strcmp(command_buffer, "help") == 0) {
            nano_print("Comenzi interne: help, exit\n");
            nano_print("Programe externe: date, time, mandel\n");
        }
		else if (strcmp(command_buffer, "halt") == 0) {
            nano_print("Nano OS se inchide...\n");
			nano_shutdown();
				
				// Dacă din orice motiv nu se închide instant, oprim execuția
				while(1) {
					__asm__ volatile ("hlt");
				}
				return 0;
            break;
        } 
        else if (starts_with(command_buffer, "cd ")) {
            const char* path = command_buffer + 3;

            if (nano_cd(path)) {
                //nano_print("Director schimbat.\n");
            } else {
                nano_print("cd: No such file or directory.\n");
            }
        }
        
        else {
            // Trecem la rând nou înainte de rularea programului extern
            //nano_print("\n");
			
            // 1. Verificăm dacă utilizatorul a pus '&' la sfârșitul liniei
            int len = 0;
            while (command_buffer[len] != '\0') len++;
            
            int background = 0;
            // Trecem peste spațiile de la final
            while (len > 0 && (command_buffer[len - 1] == ' ' || command_buffer[len - 1] == '\t')) {
                len--;
            }
            
            if (len > 0 && command_buffer[len - 1] == '&') {
                background = 1;
                command_buffer[len - 1] = '\0'; // Tăiem '&' din șir
            }
			
            int success = nano_exec(command_buffer);
            if (!success) {
                nano_print("Comanda necunoscuta: ");
                nano_print(command_buffer);
                nano_print("\n");
            }else{
				// 2. Așteptăm doar dacă NU e în background!
                if (!background) {
                    nano_wait();
                } else {
                    nano_print("[Background task started]\n");
                }
			}
        }
    }

    return 0;
}