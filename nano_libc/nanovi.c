#include "nano_libc.h"

#define SCREEN_ROWS 24
#define SCREEN_COLS 80

// Tipurile de moduri disponibile în editorul vi
typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_DELETE_PENDING,
    MODE_VISUAL,
    MODE_YANK_PENDING // Stare temporară pentru comanda yy
} EditorMode;

// Structura principală a stării editorului
typedef struct {
    char lines[100][80]; // Maxim 100 de linii, 80 caractere per linie
    int num_lines;
    int cx, cy;          // Coordonatele cursorului X și Y
    EditorMode mode;
    char filename[32];
    int modified;        // Flag-ul "dirty" (1 dacă fișierul a fost modificat și nesalvat)
    char status_msg[64]; // Mesajul afișat temporar pe bara de stare
    
    // Coordonate pentru Modul Vizual (selecție text)
    int visual_start_x;
    int visual_start_y;
} Editor;

Editor E;

int last_key = 0;

// Buffer pentru linia de comandă (când se apasă tasta ':')
char cmd_input[32];
int cmd_input_len = 0;

// Clipboard global pentru operațiunile de Yank și Put
char clipboard[1024];
int clipboard_len = 0;

// Funcție helper pentru conversia unui număr în șir de caractere
void int_to_str(int n, char* buf) {
    itoa(n, buf, 10);
}

// Verifică dacă o poziție (row, col) se află în interiorul selecției vizuale
int is_in_selection(int row, int col) {
    if (E.mode != MODE_VISUAL) return 0;

    int start_y = E.visual_start_y;
    int start_x = E.visual_start_x;
    int end_y = E.cy;
    int end_x = E.cx;

    // Normalizăm coordonatele indiferent de direcția în care s-a făcut selecția
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int ty = start_y; start_y = end_y; end_y = ty;
        int tx = start_x; start_x = end_x; end_x = tx;
    }

    if (row > start_y && row < end_y) return 1;
    if (row == start_y && row == end_y) {
        return (col >= start_x && col <= end_x);
    }
    if (row == start_y) {
        return (col >= start_x);
    }
    if (row == end_y) {
        return (col <= end_x);
    }

    return 0;
}

// Funcția de reîmprospătare și randare a ecranului
void editor_refresh_screen() {
    nano_clear_screen();

    // 1. Afișarea liniilor de text din buffer
    for (int i = 0; i < SCREEN_ROWS - 2; i++) {
        if (i < E.num_lines) {
            if (E.mode == MODE_VISUAL) {
                int len = strlen(E.lines[i]);
                int in_highlight = 0;
                for (int j = 0; j <= len; j++) {
                    int should_highlight = is_in_selection(i, j);
                    if (should_highlight && !in_highlight) {
                        nano_print("\033[7m"); // Activează Reverse Video (Highlight)
                        in_highlight = 1;
                    } else if (!should_highlight && in_highlight) {
                        nano_print("\033[0m"); // Resetează stilul ANSI
                        in_highlight = 0;
                    }
                    if (j < len) {
                        char ch_str[2] = {E.lines[i][j], '\0'};
                        nano_print(ch_str);
                    }
                }
                if (in_highlight) {
                    nano_print("\033[0m");
                }
                nano_print("\n");
            } else {
                nano_print(E.lines[i]);
                nano_print("\n");
            }
        } else {
            nano_print("~\n");
        }
    }

    // 2. Afișarea barei de stare sau a mesajelor temporare
    if (E.status_msg[0] != '\0') {
        nano_print(E.status_msg);
    } else {
        if (E.mode == MODE_NORMAL) {
            nano_print("-- NORMAL -- ");
            if (E.modified) nano_print("[+] ");
            nano_print("File: ");
            nano_print(E.filename);
        } else if (E.mode == MODE_INSERT) {
            nano_print("-- INSERT -- ");
            if (E.modified) nano_print("[+] ");
            nano_print("File: ");
            nano_print(E.filename);
        } else if (E.mode == MODE_VISUAL) {
            nano_print("-- VISUAL -- (Press y to Yank) File: ");
            nano_print(E.filename);
        } else if (E.mode == MODE_DELETE_PENDING) {
            nano_print("-d- (waiting for d or w) File: ");
            nano_print(E.filename);
        } else if (E.mode == MODE_YANK_PENDING) {
            nano_print("-y- (waiting for y for yy) File: ");
            nano_print(E.filename);
        } else if (E.mode == MODE_COMMAND) {
            nano_print(":");
            nano_print(cmd_input);
        }
    }
    nano_print("\n");

    // 3. Poziționarea fizică a cursorului în terminal folosind secvențe ANSI
    char cursor_seq[32];
    if (E.mode == MODE_COMMAND) {
        sprintf(cursor_seq, "\033[%d;%dH", SCREEN_ROWS, cmd_input_len + 2);
    } else {
        sprintf(cursor_seq, "\033[%d;%dH", E.cy + 1, E.cx + 1);
    }
    nano_print(cursor_seq);
}

// Salvarea conținutului editorului în fișierul de pe disc
void editor_save_file() {
    char disk_buffer[4096];
    int offset = 0;
    
    for (int i = 0; i < E.num_lines; i++) {
        int len = strlen(E.lines[i]);
        for (int j = 0; j < len; j++) {
            if (offset < (int)sizeof(disk_buffer) - 2) {
                disk_buffer[offset++] = E.lines[i][j];
            }
        }
        if (offset < (int)sizeof(disk_buffer) - 2) {
            disk_buffer[offset++] = '\n';
        }
    }
    disk_buffer[offset] = '\0';
    
    // Verificăm dacă scrierea a returnat un număr de biți > 0 (Succes)
    if (nano_write_file(E.filename, (uint8_t*)disk_buffer, offset) > 0) {
        E.modified = 0; 
        strcpy(E.status_msg, "\"");
        strcat(E.status_msg, E.filename);
        strcat(E.status_msg, "\" [Saved]");
    } else {
        strcpy(E.status_msg, "Error saving file!");
    }
}

int main(int argc, char* argv[]) {
    // Preluarea numefui de fișier din argumente sau setarea unui fallback
    if (argc >= 2) {
        strcpy(E.filename, argv[1]);
    } else {
        strcpy(E.filename, "test.txt");
    }

    E.cx = 0;
    E.cy = 0;
    E.mode = MODE_NORMAL;
    E.num_lines = 1;
    E.modified = 0;
    E.status_msg[0] = '\0';
    memset(E.lines[0], 0, 80);

    // Citirea fișierului de pe disc la pornire
    uint8_t file_buffer[4096];
    int bytes_read = nano_read_file(E.filename, file_buffer, sizeof(file_buffer) - 1);
    
    if (bytes_read > 0) {
        file_buffer[bytes_read] = '\0';
        int line_idx = 0;
        int col_idx = 0;
        
        for (int i = 0; i < bytes_read; i++) {
            if (file_buffer[i] == '\n') {
                E.lines[line_idx][col_idx] = '\0';
                line_idx++;
                col_idx = 0;
                if (line_idx >= 100) break;
            } else if (file_buffer[i] != '\r') {
                if (col_idx < 79) {
                    E.lines[line_idx][col_idx++] = file_buffer[i];
                }
            }
        }
        E.lines[line_idx][col_idx] = '\0';
        E.num_lines = line_idx + 1;
    } else {
        nano_create_file(E.filename, 1024);
    }

    // Bucla principală a editorului
    while (1) {
        editor_refresh_screen();
        
        char c = nano_read_char();
        last_key = (int)c;
        
        if (E.mode != MODE_COMMAND) {
            E.status_msg[0] = '\0';
        }
        
        // Gestionarea intrărilor în funcție de modul curent
        if (E.mode == MODE_NORMAL) {
            switch (c) {
                case 'i': 
                    E.mode = MODE_INSERT; 
                    break;
                case 'v': 
                    E.mode = MODE_VISUAL;
                    E.visual_start_x = E.cx;
                    E.visual_start_y = E.cy;
                    break;
                case 'y': 
                    E.mode = MODE_YANK_PENDING;
                    break;
                case 'p': { // Lipire (Put) din clipboard
                    if (clipboard_len > 0) {
                        int len = strlen(E.lines[E.cy]);
                        if (len + clipboard_len < SCREEN_COLS - 1) {
                            for (int i = len; i >= E.cx; i--) {
                                E.lines[E.cy][i + clipboard_len] = E.lines[E.cy][i];
                            }
                            for (int i = 0; i < clipboard_len; i++) {
                                E.lines[E.cy][E.cx + i] = clipboard[i];
                            }
                            E.cx += clipboard_len;
                            E.modified = 1;
                            strcpy(E.status_msg, "Text pasted");
                        }
                    }
                    break;
                }
                case 'h': // Deplasare stânga
                    if (E.cx > 0) E.cx--; 
                    break;
                case 'l': // Deplasare dreapta
                    if (E.cx < strlen(E.lines[E.cy])) E.cx++; 
                    break;
                case 'j': // Deplasare jos
                    if (E.cy < E.num_lines - 1) E.cy++; 
                    if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
                    break;
                case 'k': // Deplasare sus
                    if (E.cy > 0) E.cy--; 
                    if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
                    break;
                case 'd': 
                    E.mode = MODE_DELETE_PENDING;
                    break;
                case 'x': { // Ștergere caracter curent (stil vi)
                    int len = strlen(E.lines[E.cy]);
                    if (E.cx < len) {
                        for (int i = E.cx; i < len; i++) {
                            E.lines[E.cy][i] = E.lines[E.cy][i + 1];
                        }
                        int new_len = strlen(E.lines[E.cy]);
                        if (E.cx >= new_len && E.cx > 0) {
                            E.cx--;
                        }
                        E.modified = 1;
                    }
                    break;
                }
                case 's':
                    editor_save_file();
                    break;
                case ':':
                    E.mode = MODE_COMMAND;
                    cmd_input_len = 0;
                    cmd_input[0] = '\0';
                    E.status_msg[0] = '\0';
                    break;
            }
        } 
        else if (E.mode == MODE_YANK_PENDING) {
            if (c == 'y') {
                // YY: Copierea liniei curente întregi
                strcpy(clipboard, E.lines[E.cy]);
                int len = strlen(clipboard);
                clipboard[len] = '\n';
                clipboard[len + 1] = '\0';
                clipboard_len = len + 1;

                strcpy(E.status_msg, "Line yanked (yy)");
            }
            E.mode = MODE_NORMAL;
        }
        else if (E.mode == MODE_VISUAL) {
            switch (c) {
                case 27: 
                case 'v':
                    E.mode = MODE_NORMAL;
                    break;
                case 'h': 
                    if (E.cx > 0) E.cx--; 
                    break;
                case 'l': 
                    if (E.cx < strlen(E.lines[E.cy])) E.cx++; 
                    break;
                case 'j': 
                    if (E.cy < E.num_lines - 1) E.cy++; 
                    if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
                    break;
                case 'k': 
                    if (E.cy > 0) E.cy--; 
                    if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
                    break;
                case 'y': { // Yank text selectat vizual
                    int start_y = E.visual_start_y;
                    int start_x = E.visual_start_x;
                    int end_y = E.cy;
                    int end_x = E.cx;

                    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
                        int ty = start_y; start_y = end_y; end_y = ty;
                        int tx = start_x; start_x = end_x; end_x = tx;
                    }

                    clipboard_len = 0;
                    for (int y = start_y; y <= end_y; y++) {
                        int row_len = strlen(E.lines[y]);
                        int col_start = (y == start_y) ? start_x : 0;
                        int col_end = (y == end_y) ? end_x : row_len;

                        for (int x = col_start; x <= col_end && x < row_len; x++) {
                            if (clipboard_len < (int)sizeof(clipboard) - 1) {
                                clipboard[clipboard_len++] = E.lines[y][x];
                            }
                        }
                        if (y < end_y && clipboard_len < (int)sizeof(clipboard) - 1) {
                            clipboard[clipboard_len++] = '\n';
                        }
                    }
                    clipboard[clipboard_len] = '\0';
                    E.mode = MODE_NORMAL;
                    strcpy(E.status_msg, "Text yanked");
                    break;
                }
            }
        }
        else if (E.mode == MODE_DELETE_PENDING) {
            if (c == 'd') {
                // DD: Ștergere linie întreagă
                if (E.num_lines > 1) {
                    for (int i = E.cy; i < E.num_lines - 1; i++) {
                        strcpy(E.lines[i], E.lines[i + 1]);
                    }
                    E.num_lines--;
                    if (E.cy >= E.num_lines) {
                        E.cy = E.num_lines - 1;
                    }
                } else {
                    E.lines[0][0] = '\0';
                }
                E.cx = 0;
                E.modified = 1;
            } else if (c == 'w') {
                // DW: Ștergere cuvânt
                char* line = E.lines[E.cy];
                int len = strlen(line);
                if (E.cx < len) {
                    int i = E.cx;
                    if (line[i] == ' ' || line[i] == '\t') {
                        while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
                    } else {
                        while (i < len && line[i] != ' ' && line[i] != '\t') i++;
                        while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
                    }
                    int count_to_delete = i - E.cx;
                    for (int j = E.cx; j <= len - count_to_delete; j++) {
                        line[j] = line[j + count_to_delete];
                    }
                    E.modified = 1;
                }
            }
            E.mode = MODE_NORMAL;
        }
        else if (E.mode == MODE_INSERT) {
            if (c == 27 || c == 1) { 
                E.mode = MODE_NORMAL;
            } else if (c == '\n' || c == '\r') {
                // Inserare linie nouă (Enter)
                if (E.num_lines < 98) {
                    for (int i = E.num_lines; i > E.cy + 1; i--) {
                        strcpy(E.lines[i], E.lines[i-1]);
                    }
                    E.num_lines++;
                    E.cy++;
                    E.cx = 0;
                    E.lines[E.cy][0] = '\0';
                    E.modified = 1;
                }
            } else if (c == 8 || c == 127) {
                // Backspace
                if (E.cx > 0) {
                    int len = strlen(E.lines[E.cy]);
                    for (int i = E.cx - 1; i < len; i++) {
                        E.lines[E.cy][i] = E.lines[E.cy][i + 1];
                    }
                    E.cx--;
                    E.modified = 1;
                }
            } else {
                // Inserare caracter normal
                int len = strlen(E.lines[E.cy]);
                if (len < SCREEN_COLS - 1 && E.cx < SCREEN_COLS - 1) {
                    for (int i = len; i >= E.cx; i--) {
                        E.lines[E.cy][i + 1] = E.lines[E.cy][i];
                    }
                    E.lines[E.cy][E.cx] = c;
                    E.cx++;
                    E.modified = 1;
                }
            }
        }
        else if (E.mode == MODE_COMMAND) {
            if (c == 27 || c == 1) { 
                E.mode = MODE_NORMAL;
            } else if (c == '\n' || c == '\r') {
                // Procesarea comenzilor introduse în linia de jos
                if (strcmp(cmd_input, "q") == 0) {
                    if (E.modified) {
                        strcpy(E.status_msg, "No write since last change (add ! to override)");
                        E.mode = MODE_NORMAL;
                    } else {
                        nano_clear_screen();
                        return 0;
                    }
                } else if (strcmp(cmd_input, "q!") == 0) {
                    nano_clear_screen();
                    return 0;
                } else if (strcmp(cmd_input, "wq") == 0) {
                    editor_save_file();
                    if (!E.modified) {
                        nano_clear_screen();
                        return 0;
                    }
                    E.mode = MODE_NORMAL;
                } else {
                    strcpy(E.status_msg, "Unknown command");
                    E.mode = MODE_NORMAL;
                }
            } else if (c == 8 || c == 127) {
                if (cmd_input_len > 0) {
                    cmd_input_len--;
                    cmd_input[cmd_input_len] = '\0';
                }
            } else {
                if (cmd_input_len < 30) {
                    cmd_input[cmd_input_len++] = c;
                    cmd_input[cmd_input_len] = '\0';
                }
            }
        }
    }

    return 0;
}