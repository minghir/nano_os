#include "nano_libc.h"
#include "stdio.h"
#include "string.h"

// Adăugăm structura exact cum ai cerut

int SCREEN_ROWS = 24;
int SCREEN_COLS = 80;

#define MAX_UNDO_STEPS 30

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_DELETE_PENDING,
    MODE_VISUAL,
    MODE_YANK_PENDING,
    MODE_SEARCH
} EditorMode;

typedef struct {
    char** lines;        
    int num_lines;       
    int capacity;        
    int cx, cy;          
    int row_offset;      
    int col_offset;      
    EditorMode mode;
    char filename[32];
    int modified;
    char status_msg[64];
    
    int visual_start_x;
    int visual_start_y;
    int show_line_numbers;
} Editor;

Editor E;

typedef struct {
    char** lines;
    int num_lines;
    int cx, cy;
} EditorState;

int undo_recorded_in_session = 0;

EditorState undo_stack[MAX_UNDO_STEPS];
int undo_count = 0;

EditorState redo_stack[MAX_UNDO_STEPS];
int redo_count = 0;


int last_key = 0;
char cmd_input[32];
int cmd_input_len = 0;
char clipboard[1024];
int clipboard_len = 0;

// Variabile globale pentru căutare
char search_query[32];
int search_query_len = 0;

void int_to_str(int n, char* buf) {
    itoa(n, buf, 10);
}

// Eliberează memoria unui snapshot
void editor_free_state(EditorState* state) {
    if (state->lines) {
        for (int i = 0; i < state->num_lines; i++) {
            nano_free(state->lines[i]);
        }
        nano_free(state->lines);
        state->lines = NULL;
    }
}

// Salvează starea curentă înainte de o modificare
void editor_push_undo() {
    // Dacă stiva e plină, o eliberăm pe cea mai veche și mutăm totul la stânga
    if (undo_count >= MAX_UNDO_STEPS) {
        editor_free_state(&undo_stack[0]);
        for (int i = 0; i < MAX_UNDO_STEPS - 1; i++) {
            undo_stack[i] = undo_stack[i + 1];
        }
        undo_count = MAX_UNDO_STEPS - 1;
    }

    EditorState state;
    state.num_lines = E.num_lines;
    state.cx = E.cx;
    state.cy = E.cy;
    state.lines = (char**)nano_malloc(E.num_lines * sizeof(char*));
    
    for (int i = 0; i < E.num_lines; i++) {
        int len = (int)strlen(E.lines[i]);
        state.lines[i] = (char*)nano_malloc(len + 1);
        strcpy(state.lines[i], E.lines[i]);
    }

    undo_stack[undo_count++] = state;

    // Când facem o acțiune nouă, golim stiva de Redo
    for (int i = 0; i < redo_count; i++) {
        editor_free_state(&redo_stack[i]);
    }
    redo_count = 0;
}

void editor_undo() {
    if (undo_count == 0) {
        strcpy(E.status_msg, "Already at oldest change");
        return;
    }

    // Salvăm starea curentă în redo_stack înainte de a aplica undo
    if (redo_count >= MAX_UNDO_STEPS) {
        editor_free_state(&redo_stack[0]);
        for (int i = 0; i < MAX_UNDO_STEPS - 1; i++) {
            redo_stack[i] = redo_stack[i + 1];
        }
        redo_count = MAX_UNDO_STEPS - 1;
    }

    EditorState current_state;
    current_state.num_lines = E.num_lines;
    current_state.cx = E.cx;
    current_state.cy = E.cy;
    current_state.lines = (char**)nano_malloc(E.num_lines * sizeof(char*));
    for (int i = 0; i < E.num_lines; i++) {
        int len = (int)strlen(E.lines[i]);
        current_state.lines[i] = (char*)nano_malloc(len + 1);
        strcpy(current_state.lines[i], E.lines[i]);
    }
    redo_stack[redo_count++] = current_state;

    // Luăm starea anterioară din undo_stack
    EditorState prev_state = undo_stack[--undo_count];

    // Eliberăm liniile actuale din E
    for (int i = 0; i < E.num_lines; i++) {
        nano_free(E.lines[i]);
    }
    if (E.lines) nano_free(E.lines);

    // Restaurăm în E
    E.lines = prev_state.lines;
    E.num_lines = prev_state.num_lines;
    E.cx = prev_state.cx;
    E.cy = prev_state.cy;
    E.modified = 1;

    // Corectăm cursorul dacă e cazul
    if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
    if (E.cy < 0) E.cy = 0;
    int len = (int)strlen(E.lines[E.cy]);
    if (E.cx > len) E.cx = len;

    strcpy(E.status_msg, "Undo");
}

void editor_redo() {
    if (redo_count == 0) {
        strcpy(E.status_msg, "Already at newest change");
        return;
    }

    // Salvăm starea curentă în undo_stack
    if (undo_count >= MAX_UNDO_STEPS) {
        editor_free_state(&undo_stack[0]);
        for (int i = 0; i < MAX_UNDO_STEPS - 1; i++) {
            undo_stack[i] = undo_stack[i + 1];
        }
        undo_count = MAX_UNDO_STEPS - 1;
    }

    EditorState current_state;
    current_state.num_lines = E.num_lines;
    current_state.cx = E.cx;
    current_state.cy = E.cy;
    current_state.lines = (char**)nano_malloc(E.num_lines * sizeof(char*));
    for (int i = 0; i < E.num_lines; i++) {
        int len = (int)strlen(E.lines[i]);
        current_state.lines[i] = (char*)nano_malloc(len + 1);
        strcpy(current_state.lines[i], E.lines[i]);
    }
    undo_stack[undo_count++] = current_state;

    // Luăm starea din redo_stack
    EditorState next_state = redo_stack[--redo_count];

    // Eliberăm liniile actuale din E
    for (int i = 0; i < E.num_lines; i++) {
        nano_free(E.lines[i]);
    }
    if (E.lines) nano_free(E.lines);

    // Restaurăm în E
    E.lines = next_state.lines;
    E.num_lines = next_state.num_lines;
    E.cx = next_state.cx;
    E.cy = next_state.cy;
    E.modified = 1;

    if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
    if (E.cy < 0) E.cy = 0;
    int len = (int)strlen(E.lines[E.cy]);
    if (E.cx > len) E.cx = len;

    strcpy(E.status_msg, "Redo");
}


void editor_find_next() {
    if (search_query_len == 0) {
        strcpy(E.status_msg, "No previous search");
        return;
    }

    int start_y = E.cy;
    int start_x = E.cx + 1; // Începem de la caracterul de după cursor

    for (int i = 0; i < E.num_lines; i++) {
        int check_y = (start_y + i) % E.num_lines;
        char* line = E.lines[check_y];
        int len = (int)strlen(line);

        int search_from = (check_y == start_y) ? start_x : 0;
        if (search_from < 0) search_from = 0;

        if (search_from <= len) {
            char* match = strstr(line + search_from, search_query);
            if (match) {
                E.cy = check_y;
                E.cx = (int)(match - line);
                sprintf(E.status_msg, "[Found at line %d, col %d]", check_y + 1, E.cx + 1);
                return;
            }
        }
    }
    strcpy(E.status_msg, "Pattern not found");
}

void editor_search() {
    if (search_query_len == 0) {
        E.mode = MODE_NORMAL;
        return;
    }
    editor_find_next();
    E.mode = MODE_NORMAL;
}

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

    int len = (int)strlen(s);
    E.lines[at] = (char*)nano_malloc(len + 1);
    strcpy(E.lines[at], s);
    
    E.num_lines++;
    E.modified = 1;
}

void editor_row_insert_char(int row, int at, char c) {

    if (!undo_recorded_in_session) {
        editor_push_undo();
        undo_recorded_in_session = 1;
    }

    if (row < 0 || row >= E.num_lines) return;
    if (at < 0) at = 0;
    int len = (int)strlen(E.lines[row]);
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

void editor_row_delete_char(int row, int at) {


    if (!undo_recorded_in_session) {
        editor_push_undo();
        undo_recorded_in_session = 1;
    }

    if (row < 0 || row >= E.num_lines) return;
    if (at <= 0) return;
    int len = (int)strlen(E.lines[row]);
    if (at > len) return;

    char* new_line = (char*)nano_malloc(len);
    for (int i = 0; i < at - 1; i++) new_line[i] = E.lines[row][i];
    for (int i = at; i < len; i++) new_line[i - 1] = E.lines[row][i];
    new_line[len - 1] = '\0';

    nano_free(E.lines[row]);
    E.lines[row] = new_line;
    E.modified = 1;
}

void editor_insert_newline() {


    if (!undo_recorded_in_session) {
        editor_push_undo();
        undo_recorded_in_session = 1;
    }


    if (E.cy < 0 || E.cy >= E.num_lines) return;

    char* line = E.lines[E.cy];
    int len = (int)strlen(line);
    
    if (E.cx > len) E.cx = len;
    if (E.cx < 0) E.cx = 0;

    // 1. Alocăm spațiu sigur pentru partea stângă (până la cursor)
    char* left_part = (char*)nano_malloc(E.cx + 1);
    memcpy(left_part, line, E.cx);
    left_part[E.cx] = '\0';

    // 2. Alocăm spațiu sigur pentru partea dreaptă (de la cursor la final)
    int right_len = len - E.cx;
    char* right_part = (char*)nano_malloc(right_len + 1);
    memcpy(right_part, &line[E.cx], right_len);
    right_part[right_len] = '\0';

    // 3. Eliberăm linia veche și o înlocuim cu partea stângă
    nano_free(E.lines[E.cy]);
    E.lines[E.cy] = left_part;

    // 4. Inserăm linia nouă dedesubt cu partea dreaptă
    editor_insert_row(E.cy + 1, right_part);
    
    // 5. Eliberăm buffer-ul temporar din dreapta (editor_insert_row își face propria copie)
    nano_free(right_part);

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
    
    // Scroll vertical
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + render_rows) {
        E.row_offset = E.cy - render_rows + 1;
    }

    // Scroll orizontal
    if (E.cx < E.col_offset) {
        E.col_offset = E.cx;
    }
    if (E.cx >= E.col_offset + SCREEN_COLS) {
        E.col_offset = E.cx - SCREEN_COLS + 1;
    }
}

void editor_refresh_screen() {
    editor_scroll();

    static uint16_t screen_buffer[256 * 128]; 

    int total_cells = SCREEN_COLS * SCREEN_ROWS;
    uint16_t default_attr = 0x0F00; 
    uint16_t invert_attr  = 0x0B00;

    for (int i = 0; i < total_cells; i++) {
        screen_buffer[i] = default_attr | ' ';
    }

    int render_rows = SCREEN_ROWS - 1;
    
    for (int i = 0; i < render_rows; i++) {
        int file_row = i + E.row_offset;
        int row_start_idx = i * SCREEN_COLS;
        int col = 0;

        // 1. Dacă sunt activate, desenăm numărul rândului în stânga
        if (E.show_line_numbers) {
            if (file_row < E.num_lines) {
                char num_buf[16];
                sprintf(num_buf, "%4d ", file_row + 1); // Aliniat la dreapta pe 4 caractere + spațiu
                for (int c_idx = 0; num_buf[c_idx] != '\0' && col < SCREEN_COLS; c_idx++) {
                    screen_buffer[row_start_idx + col] = default_attr | (unsigned char)num_buf[c_idx];
                    col++;
                }
            } else {
                // Pentru rândurile goale de sub sfârșitul fișierului lăsăm spațiu gol în zona de numere
                for (int c_idx = 0; c_idx < 5 && col < SCREEN_COLS; c_idx++) {
                    screen_buffer[row_start_idx + col] = default_attr | ' ';
                    col++;
                }
            }
        }

        // 2. Randarea conținutului efectiv al rândului
        if (file_row < E.num_lines) {
            int len = (int)strlen(E.lines[file_row]);
            int text_col_start = col; // Reținem unde începe textul efectiv
            int start_j = (E.col_offset < len) ? E.col_offset : len;

            for (int j = start_j; j < len && col < SCREEN_COLS; j++) {
                int should_highlight = is_in_selection(file_row, j);
                uint16_t attr = should_highlight ? invert_attr : default_attr;
                char ch = E.lines[file_row][j];
                
                if (ch == '\t') {
                    do {
                        if (col < SCREEN_COLS) {
                            screen_buffer[row_start_idx + col] = attr | ' ';
                            col++;
                        }
                    } while ((col - text_col_start) % 4 != 0 && col < SCREEN_COLS);
                } else {
                    if (col < SCREEN_COLS) {
                        screen_buffer[row_start_idx + col] = attr | (unsigned char)ch;
                        col++;
                    }
                }
            }
        } else {
            // Dacă rândul e gol și nu avem numere, afișăm '~', altfel doar lăsăm gol
            if (!E.show_line_numbers || col < SCREEN_COLS) {
                screen_buffer[row_start_idx + col] = default_attr | '~';
            }
        }
    }

    // 3. Randarea bării de stare
    int status_row_idx = (SCREEN_ROWS - 1) * SCREEN_COLS;
    for (int i = 0; i < SCREEN_COLS; i++) {
        screen_buffer[status_row_idx + i] = invert_attr | ' ';
    }

    char status_text[256];
    status_text[0] = '\0';

    if (E.mode == MODE_COMMAND) {
        sprintf(status_text, ":%s", cmd_input);
    } else if (E.mode == MODE_SEARCH) {
        sprintf(status_text, "/%s", search_query); 
    } else if (E.status_msg[0] != '\0') {
        sprintf(status_text, "%s", E.status_msg);
    } else {
        const char* mode_str = "";
        if (E.mode == MODE_NORMAL) mode_str = "-- NORMAL -- ";
        else if (E.mode == MODE_INSERT) mode_str = "-- INSERT -- ";
        else if (E.mode == MODE_VISUAL) mode_str = "-- VISUAL -- ";
        
        char mod_str[16] = "";
        if (E.modified) {
            strcpy(mod_str, "[+] ");
        }
        
        //sprintf(status_text, "%s%sFile: %s", mode_str, mod_str, E.filename);
        sprintf(status_text, "%s%sFile: %s | %d/%d, Col %d", 
                mode_str, 
                mod_str, 
                E.filename[0] != '\0' ? E.filename : "[No Name]", 
                E.cy + 1,        // Rândul curent (începe de la 1)
                E.num_lines,     // Totalul rândurilor din fișier
                E.cx + 1         // Coloana curentă (începe de la 1)
        );
    }

    int s_len = (int)strlen(status_text);
    for (int i = 0; i < s_len && i < SCREEN_COLS; i++) {
        screen_buffer[status_row_idx + i] = invert_attr | (unsigned char)status_text[i];
    }

    nano_draw_full_screen(screen_buffer);

    // 4. Update poziție cursor (ținând cont de lățimea coloanei de numere dacă e activă)
    char cursor_seq[32];
    if (E.mode == MODE_COMMAND) {
        sprintf(cursor_seq, "\033[%d;%dH", SCREEN_ROWS, cmd_input_len + 2);
    } else if (E.mode == MODE_SEARCH) {
        sprintf(cursor_seq, "\033[%d;%dH", SCREEN_ROWS, search_query_len + 2); // <--- NOU
    } else {
        int screen_y = (E.cy - E.row_offset) + 1;
        int line_num_offset = E.show_line_numbers ? 5 : 0;
        int screen_x = (E.cx - E.col_offset) + 1 + line_num_offset;
        sprintf(cursor_seq, "\033[%d;%dH", screen_y, screen_x);
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
        int len = (int)strlen(E.lines[i]);
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
    
    nano_create_file(E.filename, 0,0644);

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
    // --- PRELUARE INFORMAȚII ECRAN DINAMIC ---
    VideoModeInfo vinfo;
    if (nano_get_video_info(&vinfo) == 1) {
        if (vinfo.rows > 0 && vinfo.cols > 0) {
            SCREEN_ROWS = vinfo.rows;
            SCREEN_COLS = vinfo.cols;
        }
    }

    if (argc >= 2) {
        strcpy(E.filename, argv[1]);
    } else {
        E.filename[0] = '\0';
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
    E.show_line_numbers = 0;
    E.status_msg[0] = '\0';

    if (E.filename[0] != '\0') {
        // Aflăm dimensiunea exactă a fișierului folosind funcțiile de pe host
        FILE* f_check = fopen(E.filename, "rb");
        if (f_check) {
            fseek(f_check, 0, SEEK_END);
            long file_size = ftell(f_check);
            rewind(f_check);
            fclose(f_check);

            // Alocăm memorie dinamic exact pentru tot fișierul + 1 pentru terminatorul \0
            uint8_t* file_buffer = (uint8_t*)nano_malloc(file_size + 1);
            if (file_buffer) {
                int bytes_read = nano_read_file(E.filename, file_buffer, file_size);
                if (bytes_read > 0) {
                    file_buffer[bytes_read] = '\0';
                    char current_line[4096];
                    int c_idx = 0;
                    
                    for (int i = 0; i <= bytes_read; i++) {
                        if (file_buffer[i] == '\n' || file_buffer[i] == '\0') {
                            current_line[c_idx] = '\0';
                            editor_insert_row(E.num_lines, current_line);
                            c_idx = 0;
                            if (file_buffer[i] == '\0') break;
                        } else if (file_buffer[i] == '\t') {
                            // Convertim tab-urile în spații
                            int spaces = 4 - (c_idx % 4);
                            for (int s = 0; s < spaces && c_idx < (int)sizeof(current_line) - 1; s++) {
                                current_line[c_idx++] = ' ';
                            }
                        } else if (file_buffer[i] != '\r') {
                            if (c_idx < (int)sizeof(current_line) - 1) {
                                current_line[c_idx++] = file_buffer[i];
                            }
                        }
                    }
                }
                nano_free(file_buffer);
            }
        } else {
            // Dacă fișierul nu există încă (fișier nou), va porni gol
            strcpy(E.status_msg, "[New File]");
        }
    }
    
    if (E.num_lines == 0) {
        editor_insert_row(0, "");
    }
    
    E.modified = 0;
    
    // Salvăm starea inițială ca punct de plecare pentru Undo
    editor_push_undo();

    while (1) {
        editor_refresh_screen();
        
        unsigned char c = (unsigned char)nano_read_char();
        last_key = (int)c;
        
        if (E.mode != MODE_COMMAND) {
            E.status_msg[0] = '\0';
        }
        
        if (c == 27) {  
            if (E.mode == MODE_INSERT || E.mode == MODE_VISUAL || E.mode == MODE_COMMAND) {
                E.mode = MODE_NORMAL;
                undo_recorded_in_session = 0; // Resetăm la ieșirea din insert
                nano_set_cursor_shape(0);
            }
            continue;
        }
        else if (c == 128) { 
            if (E.cy > 0) E.cy--;
            if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 129) { 
            if (E.cy < E.num_lines - 1) E.cy++;
            if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 130) { 
            if (E.cx < (int)strlen(E.lines[E.cy])) E.cx++;
            continue;
        }
        else if (c == 131) { 
            if (E.cx > 0) E.cx--;
            continue;
        }
        else if (c == 132) { 
            int len = (int)strlen(E.lines[E.cy]);
            if (E.cx < len) {
                char* line = E.lines[E.cy];
                for (int i = E.cx; i < len; i++) {
                    line[i] = line[i + 1];
                }
                E.modified = 1;
            }
            continue;
        }
        else if (c == 133) { 
            E.cx = 0;
            continue;
        }
        else if (c == 135) { 
            E.cx = (int)strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 134) { 
            E.cy -= (SCREEN_ROWS - 1);
            if (E.cy < 0) E.cy = 0;
            if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 136) { 
            E.cy += (SCREEN_ROWS - 1);
            if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
            if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
            continue;
        }
        else if (c == 137) { 
            nano_set_cursor_shape(1);
            E.mode = MODE_INSERT;
            continue;
        }

        if (E.mode == MODE_NORMAL) {
            switch (c) {
                case 'i': 
                    undo_recorded_in_session = 0;
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
                    editor_push_undo(); // Salvăm starea înainte de paste
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
                    if (E.cx < (int)strlen(E.lines[E.cy])) E.cx++; 
                    break;
                case 'j': 
                    if (E.cy < E.num_lines - 1) E.cy++; 
                    if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
                    break;
                case 'k': 
                    if (E.cy > 0) E.cy--; 
                    if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
                    break;
                case 'd': 
                    E.mode = MODE_DELETE_PENDING;
                    break;
                case 'x': { 
                    editor_push_undo(); // Salvăm starea înainte de ștergere
                    int len = (int)strlen(E.lines[E.cy]);
                    if (E.cx < len) {
                        editor_row_delete_char(E.cy, E.cx + 1);
                        int new_len = (int)strlen(E.lines[E.cy]);
                        if (E.cx >= new_len && E.cx > 0) {
                            E.cx--;
                        }
                    }
                    break;
                }
                case 's':
                    editor_save_file();
                    break;
                case '/': 
                    E.mode = MODE_SEARCH;
                    search_query_len = 0;
                    search_query[0] = '\0';
                    E.status_msg[0] = '\0';
                    break;
                case 'n': 
                    editor_find_next();
                    break;
                case 'u':
                    editor_undo();
                    break;
                case 'R':
                    editor_redo();
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
                int len = (int)strlen(clipboard);
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
                    if (E.cx < (int)strlen(E.lines[E.cy])) E.cx++; 
                    break;
                case 'j': 
                    if (E.cy < E.num_lines - 1) E.cy++; 
                    if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
                    break;
                case 'k': 
                    if (E.cy > 0) E.cy--; 
                    if (E.cx > (int)strlen(E.lines[E.cy])) E.cx = (int)strlen(E.lines[E.cy]);
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
                        int row_len = (int)strlen(E.lines[y]);
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
                editor_push_undo(); // Salvăm starea înainte de a șterge linia
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
            } else if (c == '\t' || c == 9) { 
                int spaces = 4 - (E.cx % 4); // Aliniere la oprirea de tab (tab stop)
                for (int i = 0; i < spaces; i++) {
                    editor_row_insert_char(E.cy, E.cx, ' ');
                    E.cx++;
                }
            }
			else {
                editor_row_insert_char(E.cy, E.cx, c);
                E.cx++;
            }
        }
        else if (E.mode == MODE_SEARCH) {
            if (c == '\n' || c == '\r') {
                editor_search();
            } else if (c == 8 || c == 127) { // Backspace
                if (search_query_len > 0) {
                    search_query_len--;
                    search_query[search_query_len] = '\0';
                }
            } else if (c == 27) { // ESC - anulează căutarea
                E.mode = MODE_NORMAL;
                E.status_msg[0] = '\0';
            } else {
                if (search_query_len < 30) {
                    search_query[search_query_len++] = (char)c;
                    search_query[search_query_len] = '\0';
                }
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
                    char* fname = &cmd_input[2];
                    while (*fname == ' ') fname++;
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
                } 
                /////////
                else if (strncmp(cmd_input, "e ", 2) == 0) {
                    char* fname = &cmd_input[2];
                    while (*fname == ' ') fname++;
                    if (*fname != '\0') {
                        // 1. Eliberăm liniile vechi din memorie
                        for (int i = 0; i < E.num_lines; i++) {
                            nano_free(E.lines[i]);
                        }
                        if (E.lines) {
                            nano_free(E.lines);
                            E.lines = NULL;
                        }
                        E.num_lines = 0;
                        E.capacity = 0;

                        // 2. Setăm noul nume de fișier
                        strcpy(E.filename, fname);
                        E.cx = 0;
                        E.cy = 0;
                        E.row_offset = 0;
                        E.col_offset = 0;

                        // 3. Citim noul fișier
                        uint32_t max_file_size = 65536;
                        uint8_t* file_buffer = (uint8_t*)nano_malloc(max_file_size);
                        if (file_buffer) {
                            int bytes_read = nano_read_file(E.filename, file_buffer, max_file_size - 1);
                            if (bytes_read > 0) {
                                file_buffer[bytes_read] = '\0';
                                char current_line[4096];
                                int c_idx = 0;
                                
                                for (int i = 0; i <= bytes_read; i++) {
                                    if (file_buffer[i] == '\n' || file_buffer[i] == '\0') {
                                        current_line[c_idx] = '\0';
                                        editor_insert_row(E.num_lines, current_line);
                                        c_idx = 0;
                                        if (file_buffer[i] == '\0') break;
                                    } else if (file_buffer[i] == '\t') {
                                        int spaces = 4 - (c_idx % 4);
                                        for (int s = 0; s < spaces && c_idx < (int)sizeof(current_line) - 1; s++) {
                                            current_line[c_idx++] = ' ';
                                        }
                                    } else if (file_buffer[i] != '\r') {
                                        if (c_idx < (int)sizeof(current_line) - 1) {
                                            current_line[c_idx++] = file_buffer[i];
                                        }
                                    }
                                }
                            } else {
                                editor_insert_row(0, "");
                                strcpy(E.status_msg, "[New File]");
                            }
                            nano_free(file_buffer);
                        }

                        if (E.num_lines == 0) {
                            editor_insert_row(0, "");
                        }
                        E.modified = 0;
                        strcpy(E.status_msg, "File loaded");
                    } else {
                        strcpy(E.status_msg, "No file name");
                    }
                    E.mode = MODE_NORMAL;
                }
                else if (strcmp(cmd_input, "set nu") == 0) {
                    E.show_line_numbers = 1;
                    strcpy(E.status_msg, "Line numbers enabled");
                    E.mode = MODE_NORMAL;
                } else if (strcmp(cmd_input, "set nonu") == 0) {
                    E.show_line_numbers = 0;
                    strcpy(E.status_msg, "Line numbers disabled");
                    E.mode = MODE_NORMAL;
                }
                else {
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