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
        // Tab (Completare automată)
        else if (c == '\t') {
            // 1. Extragem ultimul cuvânt tastat
            int start = pos;
            while (start > 0 && buffer[start - 1] != ' ' && buffer[start - 1] != '\t') {
                start--;
            }
            
            char token[128];
            int t_len = pos - start;
            if (t_len > 127) t_len = 127;
            
            for (int i = 0; i < t_len; i++) {
                token[i] = buffer[start + i];
            }
            token[t_len] = '\0';

            // 2. Separăm calea de prefixul de căutare (căutăm ultimul '/')
            int last_slash = -1;
            for (int i = 0; token[i] != '\0'; i++) {
                if (token[i] == '/') last_slash = i;
            }

            char search_prefix[64];
            char path_prefix[64] = "";
            
            if (last_slash != -1) {
                // Copiem partea de cale (inclusiv '/')
                for (int i = 0; i <= last_slash; i++) {
                    path_prefix[i] = token[i];
                }
                path_prefix[last_slash + 1] = '\0';

                // Copiem prefixul de căutare (ce e după '/')
                int p_idx = 0;
                for (int i = last_slash + 1; token[i] != '\0'; i++) {
                    search_prefix[p_idx++] = token[i];
                }
                search_prefix[p_idx] = '\0';
            } else {
                // Fără cale, căutăm direct în directorul curent
                int i = 0;
                while (token[i] != '\0' && i < 63) {
                    search_prefix[i] = token[i];
                    i++;
                }
                search_prefix[i] = '\0';
            }

            // 3. Căutăm fișierul care începe cu `search_prefix`
            char filename[64];
            int index = 0;
            char matched_name[64] = "";
            int s_len = strlen(search_prefix);

            while (nano_get_file_name_at(index, filename, sizeof(filename))) {
                if (starts_with(filename, search_prefix)) {
                    string_copy(matched_name, filename);
                    break;
                }
                index++;
            }

            // 4. Dacă am găsit o potrivire, o completăm în buffer
            if (matched_name[0] != '\0') {
                int match_len = strlen(matched_name);
                
                // Completăm doar caracterele care lipsesc din prefixul de căutare
                for (int i = s_len; i < match_len; i++) {
                    char ch = matched_name[i];
                    if (len < max_len - 1) {
                        for (int k = len; k > pos; k--) {
                            buffer[k] = buffer[k - 1];
                        }
                        buffer[pos] = ch;
                        len++;
                        pos++;
                        buffer[len] = '\0';
                    }
                }

                // Redesenăm caracterele noi pe ecran
                for (int i = pos - (match_len - s_len); i < len; i++) {
                    char s[2] = {buffer[i], '\0'};
                    nano_print(s);
                }
            }
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
        // Săgeata Sus (Istoric anterior)
        else if (c == 128) {
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
        // Săgeata Jos (Istoric următor)
        else if (c == 129) {
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
        // Săgeata Dreapta
        else if (c == 130) {
            if (pos < len) {
                pos++;
                nano_print("\033[C");
            }
        }
        // Săgeata Stânga
        else if (c == 131) {
            if (pos > 0) {
                pos--;
                nano_print("\033[D");
            }
        }
        // HOME
        else if (c == 133) {
            while (pos > 0) {
                pos--;
                nano_print("\033[D");
            }
        }
        // END
        else if (c == 135) {
            while (pos < len) {
                pos++;
                nano_print("\033[C");
            }
        }
        // Delete
        else if (c == 132) {
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
        // Caractere normale imprimabile
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
		else if (strcmp(cmd, "mount") == 0) {
			if (argc > 1) {
				if (!nano_mount(argv[1])) {
					nano_print("mount: failed to switch filesystem: ");
					nano_print(argv[1]);
					nano_print("\n");
				} else {
					nano_print("Successfully mounted: ");
					nano_print(argv[1]);
					nano_print("\n");
				}
			} else {
				nano_print("mount: missing argument (usage: mount v3 / mount nan2)\n");
			}
		}
		else if (strcmp(cmd, "ls") == 0) {
            if (argc > 1) {
                nano_ls(argv[1]);
            } else {
                nano_ls(""); // Dacă nu dăm argument, afișează directorul curent
            }
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            if (argc > 1) {
                if (!nano_mkdir(argv[1])) {
                    nano_print("mkdir: failed to create directory: ");
                    nano_print(argv[1]);
                    nano_print("\n");
                }
            } else {
                nano_print("mkdir: missing argument\n");
            }
        }
        else if (strcmp(cmd, "touch") == 0) {
            if (argc > 1) {
                // Creăm un fișier cu dimensiunea 0
                if (!nano_create_file(argv[1], 0)) {
                    nano_print("touch: failed to create file: ");
                    nano_print(argv[1]);
                    nano_print("\n");
                }
            } else {
                nano_print("touch: missing argument\n");
            }
        }
        else if (strcmp(cmd, "rm") == 0) {
            if (argc > 1) {
                if (!nano_delete_file(argv[1])) {
                    nano_print("rm: failed to remove file/directory: ");
                    nano_print(argv[1]);
                    nano_print("\n");
                }
            } else {
                nano_print("rm: missing argument\n");
            }
        }
        else if (strcmp(cmd, "format") == 0) {
            nano_format();
            nano_print("Format instruction sent to active driver.\n");
        }
        else if (strcmp(cmd, "fdisk") == 0) {
            nano_fdisk();
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