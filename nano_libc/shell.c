#include "nano_libc.h"

#define MAX_HISTORY 10
#define MAX_CMD_LEN 128

static char history[MAX_HISTORY][MAX_CMD_LEN];
static int history_count = 0;

void readline_with_history(char* buffer, int max_len) {
    int len = 0;
    int pos = 0;
    buffer[0] = '\0';
    int browsing_pos = history_count;

    while (1) {
        unsigned char c = (unsigned char)nano_read_char();

        // Enter
        if (c == '\n' || c == '\r') {
            buffer[len] = '\0';
            nano_print("\n");
            break;
        }
        // Backspace
        else if (c == '\b' || c == 127) {
            if (pos > 0) {
                nano_print("\033[D");
                for (int i = pos - 1; i < len - 1; i++) {
                    buffer[i] = buffer[i + 1];
                }
                len--;
                pos--;
                buffer[len] = '\0';

                for (int i = pos; i < len; i++) {
                    char s[2] = {buffer[i], '\0'};
                    nano_print(s);
                }
                nano_print(" ");

                for (int i = 0; i < (len - pos) + 1; i++) {
                    nano_print("\033[D");
                }
            }
        }
        // --- NOILE CODURI UNICE TRIMISE DE KERNEL ---
        else if (c == 128) { // Săgeata Sus (Istoric anterior)
            if (history_count > 0 && browsing_pos > 0) {
                browsing_pos--;
                
                for (int i = 0; i < pos; i++) nano_print("\033[D");
                for (int i = 0; i < len; i++) nano_print(" ");
                for (int i = 0; i < len; i++) nano_print("\033[D");

                int h_len = 0;
                while (history[browsing_pos][h_len] != '\0' && h_len < max_len - 1) {
                    buffer[h_len] = history[browsing_pos][h_len];
                    h_len++;
                }
                buffer[h_len] = '\0';
                len = h_len;
                pos = len;
                
                nano_print(buffer);
            }
        }
        else if (c == 129) { // Săgeata Jos (Istoric următor)
            if (history_count > 0 && browsing_pos < history_count - 1) {
                browsing_pos++;
                
                for (int i = 0; i < pos; i++) nano_print("\033[D");
                for (int i = 0; i < len; i++) nano_print(" ");
                for (int i = 0; i < len; i++) nano_print("\033[D");

                int h_len = 0;
                while (history[browsing_pos][h_len] != '\0' && h_len < max_len - 1) {
                    buffer[h_len] = history[browsing_pos][h_len];
                    h_len++;
                }
                buffer[h_len] = '\0';
                len = h_len;
                pos = len;
                
                nano_print(buffer);
            } 
            else if (browsing_pos >= history_count - 1) {
                browsing_pos = history_count;
                for (int i = 0; i < pos; i++) nano_print("\033[D");
                for (int i = 0; i < len; i++) nano_print(" ");
                for (int i = 0; i < len; i++) nano_print("\033[D");
                
                buffer[0] = '\0';
                len = 0;
                pos = 0;
            }
        }
        else if (c == 130) { // Săgeata Dreapta
            if (pos < len) {
                pos++;
                nano_print("\033[C");
            }
        }
        else if (c == 131) { // Săgeata Stânga
            if (pos > 0) {
                pos--;
                nano_print("\033[D");
            }
        }
		else if (c == 133) { // Tasta HOME
            while (pos > 0) {
                pos--;
                nano_print("\033[D");
            }
        }
        else if (c == 135) { // Tasta END
            while (pos < len) {
                pos++;
                nano_print("\033[C");
            }
        }
        else if (c == 132) { // Tasta Delete
            if (pos < len) {
                for (int i = pos; i < len - 1; i++) {
                    buffer[i] = buffer[i + 1];
                }
                len--;
                buffer[len] = '\0';

                for (int i = pos; i < len; i++) {
                    char s[2] = {buffer[i], '\0'};
                    nano_print(s);
                }
                nano_print(" ");

                for (int i = 0; i < (len - pos) + 1; i++) {
                    nano_print("\033[D");
                }
            }
        }
        // Caractere normale imprimabile (Inclusiv inserarea în mijloc)
        else if (c >= 32 && c <= 126) {
            if (len < max_len - 1) {
                for (int i = len; i > pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[pos] = c;
                len++;
                pos++;
                buffer[len] = '\0';

                for (int i = pos - 1; i < len; i++) {
                    char s[2] = {buffer[i], '\0'};
                    nano_print(s);
                }

                for (int i = 0; i < len - pos; i++) {
                    nano_print("\033[D");
                }
            }
        }
    }
}

int main() {
    nano_print("\nNano OS User-Space Shell v1.2 (cu Istoric si Editare)\n");
    char command_buffer[MAX_CMD_LEN];
    char raw_buffer[MAX_CMD_LEN];

    while (1) {
        char cwd[64];
        for (int i = 0; i < 64; i++) cwd[i] = '\0';
        
        if (nano_getcwd(cwd, 63) <= 0 || cwd[0] == '\0') {
            string_copy(cwd, "/");
        }

        nano_print("nano:");
        nano_print(cwd);
        nano_print("# ");
        
        for (int i = 0; i < MAX_CMD_LEN; i++) {
            command_buffer[i] = '\0';
        }
        
        readline_with_history(command_buffer, MAX_CMD_LEN - 1);

        if (command_buffer[0] == '\0') {
            continue;
        }

        if (history_count == 0 || strcmp(history[history_count - 1], command_buffer) != 0) {
            if (history_count < MAX_HISTORY) {
                string_copy(history[history_count], command_buffer);
                history_count++;
            } else {
                for (int i = 0; i < MAX_HISTORY - 1; i++) {
                    string_copy(history[i], history[i + 1]);
                }
                string_copy(history[MAX_HISTORY - 1], command_buffer);
            }
        }

        int r_idx = 0;
        while (command_buffer[r_idx] != '\0' && r_idx < MAX_CMD_LEN - 1) {
            raw_buffer[r_idx] = command_buffer[r_idx];
            r_idx++;
        }
        raw_buffer[r_idx] = '\0';

        char* argv[16];
        int argc = 0;
        int p = 0;

        while (command_buffer[p] != '\0' && argc < 15) {
            while (command_buffer[p] == ' ' || command_buffer[p] == '\t') {
                command_buffer[p] = '\0';
                p++;
            }
            if (command_buffer[p] == '\0') break;

            argv[argc++] = &command_buffer[p];

            while (command_buffer[p] != '\0' && command_buffer[p] != ' ' && command_buffer[p] != '\t') {
                p++;
            }
        }
        argv[argc] = NULL;

        if (argc == 0) continue;

        int background = 0;
        if (argc > 1 && strcmp(argv[argc - 1], "&") == 0) {
            background = 1;
            argc--;
            argv[argc] = NULL;

            int rb_len = 0;
            while (raw_buffer[rb_len] != '\0') rb_len++;
            while (rb_len > 0 && (raw_buffer[rb_len - 1] == ' ' || raw_buffer[rb_len - 1] == '&')) {
                raw_buffer[rb_len - 1] = '\0';
                rb_len--;
            }
        }

        char* cmd = argv[0];

        if (strcmp(cmd, "exit") == 0) {
            nano_print("Bye bye!\n");
            break;
        } 
        else if (strcmp(cmd, "clear") == 0) {
            nano_clear_screen();
        } 
        else if (strcmp(cmd, "about") == 0) {
            nano_print("Nano OS: educational x86-64 kernel\n");
        } 
        else if (strcmp(cmd, "help") == 0) {
            nano_print("Internal: help, exit, clear, about, halt, cd\n");
            nano_print("Use UP/DOWN for history, LEFT/RIGHT to edit commands!\n");
        } 
        else if (strcmp(cmd, "halt") == 0) {
            nano_print("Nano OS is shutting down...\n");
            nano_shutdown();
            while (1) { __asm__ volatile ("hlt"); }
            return 0;
        }
		else if (strcmp(cmd, "reboot") == 0) {
			nano_print("Rebooting Nano OS...\n");
			nano_reboot();
		}
		else if (strcmp(cmd, "beep") == 0) {
			nano_print("Emitere sunet PC Speaker...\n");
			nano_beep(880, 250); // Nota La (880 Hz) timp de 250 milisecunde
		}		
        else if (strcmp(cmd, "cd") == 0) {
            if (argc > 1) {
                if (!nano_cd(argv[1])) {
                    nano_print("cd: No such file or directory: ");
                    nano_print(argv[1]);
                    nano_print("\n");
                }
            } else {
                nano_print("cd: missing argument\n");
            }
        } 
        else {
            int success = nano_exec(raw_buffer);
            if (!success) {
                nano_print("Unknown command: ");
                nano_print(cmd);
                nano_print("\n");
            } else {
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