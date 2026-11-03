
#include "nano_libc.h"

#define MAX_HISTORY 10
#define MAX_CMD_LEN 128

#define MAX_ENV_VARS 16
#define ENV_NAME_LEN 32
#define ENV_VAL_LEN 128

typedef struct {
    char name[ENV_NAME_LEN];
    char value[ENV_VAL_LEN];
    int used;
} env_var_t;

static env_var_t env_table[MAX_ENV_VARS];

static char history[MAX_HISTORY][MAX_CMD_LEN];
static int history_count = 0;

// Stocăm codul de exit returnat de ultimul proces prin nano_wait()
static int last_exit_code = 0;

// Caută o variabilă în tabelă și returnează valoarea ei (sau NULL dacă nu există)
static const char* get_env_var(const char* name) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].used && strcmp(env_table[i].name, name) == 0) {
            return env_table[i].value;
        }
    }
    return NULL;
}

// Setează o variabilă (actualizează dacă există, adaugă dacă e nouă)
static void set_env_var(const char* name, const char* value) {
    // 1. Încercăm actualizarea
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].used && strcmp(env_table[i].name, name) == 0) {
            string_copy(env_table[i].value, value);
            return;
        }
    }

    // 2. Adăugăm în primul slot liber
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (!env_table[i].used) {
            string_copy(env_table[i].name, name);
            string_copy(env_table[i].value, value);
            env_table[i].used = 1;
            return;
        }
    }

    nano_print("export: Environment table is full!\n");
}


void expand_variables(char* buffer, int max_len) {
    char temp[MAX_CMD_LEN];
    int in_idx = 0;
    int out_idx = 0;

    while (buffer[in_idx] != '\0' && out_idx < max_len - 1) {
        if (buffer[in_idx] == '$') {
            in_idx++; // Sărim peste '$'

            // Extragerea numelui variabilei (litere, cifre, '_', '?')
            char var_name[ENV_NAME_LEN];
            int v_len = 0;

            if (buffer[in_idx] == '?') {
                var_name[v_len++] = buffer[in_idx++];
            } else {
                while ((buffer[in_idx] >= 'A' && buffer[in_idx] <= 'Z') ||
                       (buffer[in_idx] >= 'a' && buffer[in_idx] <= 'z') ||
                       (buffer[in_idx] >= '0' && buffer[in_idx] <= '9') ||
                       buffer[in_idx] == '_') {
                    if (v_len < ENV_NAME_LEN - 1) {
                        var_name[v_len++] = buffer[in_idx];
                    }
                    in_idx++;
                }
            }
            var_name[v_len] = '\0';

            if (v_len == 0) {
                // $ urmat de spațiu/caracter nealfanumeric -> păstrăm '$'
                temp[out_idx++] = '$';
                continue;
            }

            // A. Verificăm tabela locală de variabile
            const char* user_val = get_env_var(var_name);

            if (user_val != NULL) {
                while (*user_val != '\0' && out_idx < max_len - 1) {
                    temp[out_idx++] = *user_val++;
                }
            }
            // B. Variabile dinamice fallback ($USER, $CWD, $?)
            else if (strcmp(var_name, "USER") == 0) {
                uint32_t uid = nano_getuid();
                const char* val = (uid == 0) ? "nano" : ((uid == 1000) ? "user" : "unknown");
                while (*val != '\0' && out_idx < max_len - 1) temp[out_idx++] = *val++;
            }
            else if (strcmp(var_name, "CWD") == 0) {
                char cwd[64] = {0};
                if (nano_getcwd(cwd, 63) <= 0 || cwd[0] == '\0') {
                    cwd[0] = '/'; cwd[1] = '\0';
                }
                const char* val = cwd;
                while (*val != '\0' && out_idx < max_len - 1) temp[out_idx++] = *val++;
            }
            else if (strcmp(var_name, "?") == 0) {
                char code_str[16];
                simple_itoa(last_exit_code, code_str);
                const char* val = code_str;
                while (*val != '\0' && out_idx < max_len - 1) temp[out_idx++] = *val++;
            }
            // C. Dacă variabila nu există deloc, se expandează în string vid (standard POSIX)
        } else {
            temp[out_idx++] = buffer[in_idx++];
        }
    }

    temp[out_idx] = '\0';
    string_copy(buffer, temp);
}

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

            int last_slash = -1;
            for (int i = 0; token[i] != '\0'; i++) {
                if (token[i] == '/') last_slash = i;
            }

            char search_prefix[64];
            char path_prefix[64] = "";
            
            if (last_slash != -1) {
                for (int i = 0; i <= last_slash; i++) {
                    path_prefix[i] = token[i];
                }
                path_prefix[last_slash + 1] = '\0';

                int p_idx = 0;
                for (int i = last_slash + 1; token[i] != '\0'; i++) {
                    search_prefix[p_idx++] = token[i];
                }
                search_prefix[p_idx] = '\0';
            } else {
                int i = 0;
                while (token[i] != '\0' && i < 63) {
                    search_prefix[i] = token[i];
                    i++;
                }
                search_prefix[i] = '\0';
            }

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

            if (matched_name[0] != '\0') {
                int match_len = strlen(matched_name);
                
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
        // Săgeata Sus
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
        // Săgeata Jos
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
        // Caractere imprimabile
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
    nano_print("\n[DEBUG] Shell started successfully!\n");
    nano_print("Nano OS User-Space Shell v1.2 (cu Istoric si Autentificare)\n");
	
    char command_buffer[MAX_CMD_LEN];
    char raw_buffer[MAX_CMD_LEN];

    while (1) {
        char cwd[64];
        for (int i = 0; i < 64; i++) cwd[i] = '\0';
        
        if (nano_getcwd(cwd, 63) <= 0 || cwd[0] == '\0') {
            string_copy(cwd, "/");
        }

        // ==========================================================
        // PROMPT DINAMIC: username@nano_os:path# SAU $
        // ==========================================================
        uint32_t current_uid = nano_getuid();
        const char* username = (current_uid == 0) ? "nano" : "user";
        char prompt_char = (current_uid == 0) ? '#' : '$';

        nano_print(username);
        nano_print("@nano_os:");
        nano_print(cwd);
        
        char sym_str[3] = {prompt_char, ' ', '\0'};
        nano_print(sym_str);
        // ==========================================================
        
        for (int i = 0; i < MAX_CMD_LEN; i++) {
            command_buffer[i] = '\0';
        }
        
        readline_with_history(command_buffer, MAX_CMD_LEN - 1);

        if (command_buffer[0] == '\0') {
            continue;
        }

        expand_variables(command_buffer, MAX_CMD_LEN - 1);

        // DETECTARE REDIRECȚIONARE OUTPUT
        char* redirect_file = NULL;
        for (int i = 0; command_buffer[i] != '\0'; i++) {
            if (command_buffer[i] == '>') {
                command_buffer[i] = '\0';
                redirect_file = &command_buffer[i + 1];
                
                while (*redirect_file == ' ' || *redirect_file == '\t') {
                    redirect_file++;
                }
                
                int r_len = strlen(redirect_file);
                while (r_len > 0 && (redirect_file[r_len - 1] == ' ' || redirect_file[r_len - 1] == '\t' || redirect_file[r_len - 1] == '\n' || redirect_file[r_len - 1] == '\r')) {
                    redirect_file[r_len - 1] = '\0';
                    r_len--;
                }
                break;
            }
        }

        if (redirect_file != NULL && redirect_file[0] != '\0') {
            nano_create_file(redirect_file, 0);
            nano_set_stdout_file(redirect_file);
        } else {
            nano_set_stdout_file(NULL);
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

        // --- PARSARE ARGV CU SUPORT PENTRU GHILIMELE ("...") ---
        char* argv[16];
        int argc = 0;
        int p = 0;

        while (command_buffer[p] != '\0' && argc < 15) {
            // Sărim spațiile și tab-urile
            while (command_buffer[p] == ' ' || command_buffer[p] == '\t') {
                command_buffer[p] = '\0';
                p++;
            }
            if (command_buffer[p] == '\0') break;

            // Dacă argumentul începe cu ghilimele (duble sau simple)
            if (command_buffer[p] == '"' || command_buffer[p] == '\'') {
                char quote = command_buffer[p];
                p++; // Sărim peste ghilimeaua de deschidere
                argv[argc++] = &command_buffer[p]; // Salvăm începutul textului

                // Parcurgem tot textul dintre ghilimele (inclusiv spațiile din interior)
                while (command_buffer[p] != '\0' && command_buffer[p] != quote) {
                    p++;
                }

                if (command_buffer[p] == quote) {
                    command_buffer[p] = '\0'; // Eliminăm ghilimeaua de închidere
                    p++;
                }
            } else {
                // Argument simplu fără ghilimele
                argv[argc++] = &command_buffer[p];
                while (command_buffer[p] != '\0' && command_buffer[p] != ' ' && command_buffer[p] != '\t') {
                    p++;
                }
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
            
            // Resetăm istoricul și mediul înainte de a ne închide
            int i = history_count;
            while (i > 0) {
                for (int j = 0; j < i - 1; j++) {
                    string_copy(history[j], history[j + 1]);
                }
                i--;
            }
            memset(env_table, 0, sizeof(env_var_t)); // Resetăm mediul
            
            // Returnăm exit code-ul acumulator (last_exit_code) din nano_wait()
            return last_exit_code;
        } 
        else if (strcmp(cmd, "clear") == 0) {
            nano_clear_screen();
        } 
        else if (strcmp(cmd, "whoami") == 0) {
            uint32_t uid = nano_getuid();
            if (uid == 0) {
                nano_print("nano\n");
            } else if (uid == 1000) {
                nano_print("user\n");
            } else {
                nano_print("unknown\n");
            }
        }
        else if (strcmp(cmd, "about") == 0) {
            nano_print("Nano OS: educational x86-64 kernel\n");
        } 
        else if (strcmp(cmd, "help") == 0) {
            nano_print("Internal: help, exit, clear, about, halt, cd, whoami, su\n");
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
            nano_beep(880, 250);
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
                nano_print("mount: missing argument\n");
            }
        }
        else if (strcmp(cmd, "ls") == 0) {
            if (argc > 1) {
                nano_ls(argv[1]);
            } else {
                nano_ls("");
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
        else if (strcmp(cmd, "export") == 0) {
            if (argc == 1) {
                // Listează toate variabilele de mediu existente
                for (int i = 0; i < MAX_ENV_VARS; i++) {
                    if (env_table[i].used) {
                        nano_print(env_table[i].name);
                        nano_print("=");
                        nano_print(env_table[i].value);
                        nano_print("\n");
                    }
                }
            } else {
                // Parcurgem argumentele primite (de la argv[1])
                for (int i = 1; i < argc; i++) {
                    char* arg = argv[i];
                    int eq_pos = -1;

                    for (int k = 0; arg[k] != '\0'; k++) {
                        if (arg[k] == '=') {
                            eq_pos = k;
                            break;
                        }
                    }

                    if (eq_pos > 0) {
                        char name[ENV_NAME_LEN];
                        char val[ENV_VAL_LEN];

                        // Spargem VAR=VAL
                        int n_i = 0;
                        for (; n_i < eq_pos && n_i < ENV_NAME_LEN - 1; n_i++) {
                            name[n_i] = arg[n_i];
                        }
                        name[n_i] = '\0';

                        int v_i = 0;
                        for (int k = eq_pos + 1; arg[k] != '\0' && v_i < ENV_VAL_LEN - 1; k++) {
                            val[v_i++] = arg[k];
                        }
                        val[v_i] = '\0';

                        set_env_var(name, val);
                    } else {
                        nano_print("export: Invalid format. Use: export VAR=value\n");
                    }
                }
            }
        }
        else {
            int child_pid = nano_exec(raw_buffer);
			if (child_pid <= 0) {
				nano_print("Unknown command: ");
				nano_print(cmd);
				nano_print("\n");
                last_exit_code = 127; // Comandă negăsită (standard POSIX)
			} else {
				if (!background) {
					last_exit_code = nano_wait(); // Așteptăm DOAR dacă s-a creat un copil valid
				} else {
					nano_print("[Background task started]\n");
                    last_exit_code = 0;
				}
			}
        }
        nano_set_stdout_file(NULL);
    }

    return 0;
}