#include "nano_libc.h"

#define SCREEN_ROWS 24
#define SCREEN_COLS 80

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_DELETE_PENDING,
    MODE_VISUAL,
    MODE_YANK_PENDING
} EditorMode;

typedef struct {
    char** lines;        // Tablou dinamic de pointeri la linii (char**)
    int num_lines;       // Numărul efectiv de linii din fișier
    int capacity;        // Capacitatea curentă a tabloului de pointeri
    int cx, cy;          
    int row_offset;      
    int col_offset;      
    EditorMode mode;
    char filename[32];
    int modified;
    char status_msg[64];
    
    int visual_start_x;
    int visual_start_y;
} Editor;

Editor E;

int last_key = 0;
char cmd_input[32];
int cmd_input_len = 0;
char clipboard[1024];
int clipboard_len = 0;

static inline int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int i = 0;
    char* ptr = str;
    
    while (format[i] != '\0') {
        if (format[i] == '%' && format[i+1] == 'd') {
            int val = va_arg(args, int);
            char buf[16];
            itoa(val, buf, 10);
            char* b = buf;
            while (*b) *ptr++ = *b++;
            i += 2;
        } else if (format[i] == '%' && format[i+1] == 's') {
            char* s = va_arg(args, char*);
            while (*s) *ptr++ = *s++;
            i += 2;
        } else if (format[i] == '%' && format[i+1] == 'c') {
            char c = (char)va_arg(args, int);
            *ptr++ = c;
            i += 2;
        } else {
            *ptr++ = format[i++];
        }
    }
    *ptr = '\0';
    va_end(args);
    return (int)(ptr - str);
}


void int_to_str(int n, char* buf) {
    itoa(n, buf, 10);
}

// Inserarea unui rând nou în mod dinamic
void editor_insert_row(int at, const char* s) {
    if (at < 0 || at > E.num_lines) return;

    if (E.num_lines >= E.capacity) {
        E.capacity = E.capacity == 0 ? 16 : E.capacity * 2;
        char** new_lines = (char**)nano_malloc(E.capacity * sizeof(char*));
        
        for (int i = 0; i < E.num_lines; i++) {
            new_lines[i] = E.lines[i];
        }
        
        if (E.lines) nano_free(E.lines);
        E.lines = new_lines;
    }

    for (int i = E.num_lines; i > at; i--) {
        E.lines[i] = E.lines[i - 1];
    }

    int len = strlen(s);
    E.lines[at] = (char*)nano_malloc(len + 1);
    strcpy(E.lines[at], s);
    
    E.num_lines++;
    E.modified = 1;
}

// Inserarea unui caracter pe o linie existentă (redimensionare dinamică a rândului)
void editor_row_insert_char(int row, int at, char c) {
    if (row < 0 || row >= E.num_lines) return;
    if (at < 0) at = 0;
    int len = strlen(E.lines[row]);
    if (at > len) at = len;

    char* new_line = (char*)nano_malloc(len + 2);
    for (int i = 0; i < at; i++) new_line[i] = E.lines[row][i];
    new_line[at] = c;
    for (int i = at; i < len; i++) new_line[i + 1] = E.lines[row][i];
    new_line[len + 1] = '\0';

    nano_free(E.lines[row]);
    E.lines[row] = new_line;
    E.modified = 1;
}

// Ștergerea unui caracter de pe o linie (Backspace)
void editor_row_delete_char(int row, int at) {
    if (row < 0 || row >= E.num_lines) return;
    if (at <= 0) return;
    int len = strlen(E.lines[row]);
    if (at > len) return;

    char* new_line = (char*)nano_malloc(len);
    for (int i = 0; i < at - 1; i++) new_line[i] = E.lines[row][i];
    for (int i = at; i < len; i++) new_line[i - 1] = E.lines[row][i];
    new_line[len - 1] = '\0';

    nano_free(E.lines[row]);
    E.lines[row] = new_line;
    E.modified = 1;
}

// Inserarea unei linii noi (Enter) care sparge rândul curent
void editor_insert_newline() {
    char* line = E.lines[E.cy];
    char left_part[256];
    int i = 0;
    while (i < E.cx && line[i] != '\0') {
        left_part[i] = line[i];
        i++;
    }
    left_part[i] = '\0';

    char* right_part = &line[E.cx];

    char* new_left = (char*)nano_malloc(strlen(left_part) + 1);
    strcpy(new_left, left_part);
    nano_free(E.lines[E.cy]);
    E.lines[E.cy] = new_left;

    editor_insert_row(E.cy + 1, right_part);
    E.cy++;
    E.cx = 0;
    E.modified = 1;
}

int is_in_selection(int row, int col) {
    if (E.mode != MODE_VISUAL) return 0;

    int start_y = E.visual_start_y;
    int start_x = E.visual_start_x;
    int end_y = E.cy;
    int end_x = E.cx;

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

void editor_scroll() {
    int render_rows = SCREEN_ROWS - 1;
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + render_rows) {
        E.row_offset = E.cy - render_rows + 1;
    }
}

void editor_refresh_screen() {
    editor_scroll();
    nano_clear_screen();

    int render_rows = SCREEN_ROWS - 1;

    for (int i = 0; i < render_rows; i++) {
        int file_row = i + E.row_offset;
        if (file_row < E.num_lines) {
            if (E.mode == MODE_VISUAL) {
                int len = strlen(E.lines[file_row]);
                int in_highlight = 0;
                for (int j = 0; j <= len; j++) {
                    int should_highlight = is_in_selection(file_row, j);
                    if (should_highlight && !in_highlight) {
                        nano_print("\033[7m");
                        in_highlight = 1;
                    } else if (!should_highlight && in_highlight) {
                        nano_print("\033[0m");
                        in_highlight = 0;
                    }
                    if (j < len) {
                        char ch_str[2] = {E.lines[file_row][j], '\0'};
                        nano_print(ch_str);
                    }
                }
                if (in_highlight) {
                    nano_print("\033[0m");
                }
                nano_print("\n");
            } else {
                nano_print(E.lines[file_row]);
                nano_print("\n");
            }
        } else {
            nano_print("~\n");
        }
    }

    nano_print("\033[24;1H                                                                                ");
    nano_print("\033[24;1H");

    if (E.mode == MODE_COMMAND) {
        nano_print(":");
        nano_print(cmd_input);
    } else if (E.status_msg[0] != '\0') {
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
        }
    }

    char cursor_seq[32];
	/*
    if (E.mode == MODE_COMMAND) {
        sprintf(cursor_seq, "\033[24;%dH", cmd_input_len + 2);
    } else {
        int screen_y = (E.cy - E.row_offset) + 1;
        sprintf(cursor_seq, "\033[%d;%dH", screen_y, E.cx + 1);
    }
    nano_print(cursor_seq);
*/
	
    
    // --- SCHIMBARE FORMĂ CURSOR ÎN FUNCȚIE DE MOD ---
	/*
    if (E.mode == MODE_INSERT) {
        nano_print("\033[6 q"); // Bară verticală (|) în modul Insert
    } else {
        nano_print("\033[2 q"); // Bloc în modul Normal / Visual / Command
    }
	*/
    // ------------------------------------------------

    if (E.mode == MODE_COMMAND) {
        sprintf(cursor_seq, "\033[24;%dH", cmd_input_len + 2);
    } else {
        int screen_y = (E.cy - E.row_offset) + 1;
        sprintf(cursor_seq, "\033[%d;%dH", screen_y, E.cx + 1);
    }
    nano_print(cursor_seq);
}

void editor_save_file() {
    if (E.filename[0] == '\0') {
        strcpy(E.status_msg, "No file name");
        return;
    }

    char* disk_buffer = (char*)nano_malloc(16384);
    if (!disk_buffer) {
        strcpy(E.status_msg, "Out of memory for saving!");
        return;
    }

    int offset = 0;
    for (int i = 0; i < E.num_lines; i++) {
        int len = strlen(E.lines[i]);
        for (int j = 0; j < len; j++) {
            if (offset < 16382) {
                disk_buffer[offset++] = E.lines[i][j];
            }
        }
        if (offset < 16382) {
            disk_buffer[offset++] = '\n';
        }
    }
    disk_buffer[offset] = '\0';
    
    // Asigurăm crearea fișierului pe disc dacă nu exista
    nano_create_file(E.filename, 1024);

    if (nano_write_file(E.filename, (uint8_t*)disk_buffer, offset) > 0) {
        E.modified = 0; 
        strcpy(E.status_msg, "\"");
        nano_strcat(E.status_msg, E.filename);
        nano_strcat(E.status_msg, "\" [Saved]");
    } else {
        strcpy(E.status_msg, "Error saving file!");
    }

    nano_free(disk_buffer);
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        strcpy(E.filename, argv[1]);
    } else {
        E.filename[0] = '\0'; // Fără fișier implicit! Rămâne gol.
    }

    E.cx = 0;
    E.cy = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.mode = MODE_NORMAL;
    E.num_lines = 0;
    E.capacity = 0;
    E.lines = 0;
    E.modified = 0;
    E.status_msg[0] = '\0';

    // Citim doar dacă s-a dat un fișier valid la pornire
    if (E.filename[0] != '\0') {
        uint8_t* file_buffer = (uint8_t*)nano_malloc(16384);
        if (file_buffer) {
            int bytes_read = nano_read_file(E.filename, file_buffer, 16383);
            if (bytes_read > 0) {
                file_buffer[bytes_read] = '\0';
                char current_line[512];
                int c_idx = 0;
                
                for (int i = 0; i <= bytes_read; i++) {
                    if (file_buffer[i] == '\n' || file_buffer[i] == '\0') {
                        current_line[c_idx] = '\0';
                        editor_insert_row(E.num_lines, current_line);
                        c_idx = 0;
                        if (file_buffer[i] == '\0') break;
                    } else if (file_buffer[i] != '\r') {
                        if (c_idx < (int)sizeof(current_line) - 1) {
                            current_line[c_idx++] = file_buffer[i];
                        }
                    }
                }
            }
            nano_free(file_buffer);
        }
    }
    
    // Dacă totuși nu avem linii (fișier gol sau deschis fără nume), punem cel puțin o linie goală
    if (E.num_lines == 0) {
        editor_insert_row(0, "");
    }
    
    E.modified = 0; // Resetăm modificarea la început
	
    while (1) {
        editor_refresh_screen();
        
        unsigned char c = (unsigned char)nano_read_char();
        last_key = (int)c;
        
        if (E.mode != MODE_COMMAND) {
            E.status_msg[0] = '\0';
        }
        
        // --- GESTIONAREA TASTELOR SPECIALE ȘI ESC ---
        if (c == 27) { 
            if (E.mode == MODE_INSERT || E.mode == MODE_VISUAL || E.mode == MODE_COMMAND) {
                E.mode = MODE_NORMAL;
				nano_set_cursor_shape(0);
            }
            continue;
        }
        else if (c == 128) { // Săgeata Sus
            if (E.cy > 0) E.cy--;
            if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 129) { // Săgeata Jos
            if (E.cy < E.num_lines - 1) E.cy++;
            if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 130) { // Săgeata Dreapta
            if (E.cx < strlen(E.lines[E.cy])) E.cx++;
            continue;
        }
        else if (c == 131) { // Săgeata Stânga
            if (E.cx > 0) E.cx--;
            continue;
        }
        else if (c == 132) { // Tasta Delete
            int len = strlen(E.lines[E.cy]);
            if (E.cx < len) {
                char* line = E.lines[E.cy];
                for (int i = E.cx; i < len; i++) {
                    line[i] = line[i + 1];
                }
                E.modified = 1;
            }
            continue;
        }
        else if (c == 133) { // Home
            E.cx = 0;
            continue;
        }
        else if (c == 135) { // End
            E.cx = strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 134) { // Page Up
            E.cy -= (SCREEN_ROWS - 1);
            if (E.cy < 0) E.cy = 0;
            if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 136) { // Page Down
            E.cy += (SCREEN_ROWS - 1);
            if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
            if (E.cx > strlen(E.lines[E.cy])) E.cx = strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 137) { // Insert
			nano_set_cursor_shape(1);
            E.mode = MODE_INSERT;
            continue;
        }

        // --- GESTIONAREA MODURILOR ---
        if (E.mode == MODE_NORMAL) {
            switch (c) {
                case 'i': 
					nano_set_cursor_shape(1);
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
                case 'p': { 
                    if (clipboard_len > 0) {
                        for (int i = 0; i < clipboard_len; i++) {
                            if (clipboard[i] == '\n') {
                                editor_insert_newline();
                            } else {
                                editor_row_insert_char(E.cy, E.cx, clipboard[i]);
                                E.cx++;
                            }
                        }
                        strcpy(E.status_msg, "Text pasted");
                    }
                    break;
                }
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
                case 'd': 
                    E.mode = MODE_DELETE_PENDING;
                    break;
                case 'x': { 
                    int len = strlen(E.lines[E.cy]);
                    if (E.cx < len) {
                        editor_row_delete_char(E.cy, E.cx + 1);
                        int new_len = strlen(E.lines[E.cy]);
                        if (E.cx >= new_len && E.cx > 0) {
                            E.cx--;
                        }
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
                case 'y': { 
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
                if (E.num_lines > 1) {
                    nano_free(E.lines[E.cy]);
                    for (int i = E.cy; i < E.num_lines - 1; i++) {
                        E.lines[i] = E.lines[i + 1];
                    }
                    E.num_lines--;
                    if (E.cy >= E.num_lines) {
                        E.cy = E.num_lines - 1;
                    }
                } else {
                    nano_free(E.lines[0]);
                    E.lines[0] = (char*)nano_malloc(1);
                    E.lines[0][0] = '\0';
                }
                E.cx = 0;
                E.modified = 1;
            }
            E.mode = MODE_NORMAL;
        }
        else if (E.mode == MODE_INSERT) {
            if (c == '\n' || c == '\r') {
                editor_insert_newline();
            } else if (c == 8 || c == 127) {
                if (E.cx > 0) {
                    editor_row_delete_char(E.cy, E.cx);
                    E.cx--;
                }
            } else {
                editor_row_insert_char(E.cy, E.cx, c);
                E.cx++;
            }
        }
        else if (E.mode == MODE_COMMAND) {
            if (c == '\n' || c == '\r') {
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
                } else if (strcmp(cmd_input, "w") == 0) {
                    editor_save_file();
                    E.mode = MODE_NORMAL;
                } else if (starts_with(cmd_input, "w ")) {
                    // Prelucrăm comanda `:w filename`
                    char* fname = &cmd_input[2];
                    while (*fname == ' ') fname++; // Ignorăm spațiile suplimentare
                    if (*fname != '\0') {
                        strcpy(E.filename, fname);
                        editor_save_file();
                    } else {
                        strcpy(E.status_msg, "No file name");
                    }
                    E.mode = MODE_NORMAL;
                } else if (strcmp(cmd_input, "wq") == 0) {
                    editor_save_file();
                    if (!E.modified) {
                        nano_clear_screen();
                        return 0;
                    }
                    E.mode = MODE_NORMAL;
                } else if (starts_with(cmd_input, "wq ")) {
                    // Prelucrăm comanda `:wq filename`
                    char* fname = &cmd_input[3];
                    while (*fname == ' ') fname++;
                    if (*fname != '\0') {
                        strcpy(E.filename, fname);
                        editor_save_file();
                        if (!E.modified) {
                            nano_clear_screen();
                            return 0;
                        }
                    } else {
                        strcpy(E.status_msg, "No file name");
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