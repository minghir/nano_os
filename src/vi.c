#include "nano_libc.h"
#include "stdio.h"
#include "string.h"

int SCREEN_ROWS = 24;
int SCREEN_COLS = 80;

// Atribute de culoare (VGA text mode attributes - combinat: Atribut << 8 | Caracter)
#define ATTR_NORMAL   (0x0F << 8) // Alb pe negru
#define ATTR_KEYWORD  (0x09 << 8) // Albastru deschis (pentru if, else, int...)
#define ATTR_STRING   (0x0A << 8) // Verde (pentru "text")
#define ATTR_COMMENT  (0x07 << 8) // Gri (pentru comentarii)
#define ATTR_NUMBER   (0x0E << 8) // Galben (pentru cifre)
#define ATTR_INVERT   (0x0B << 8) // Cyan/Invert pentru status bar și selecție

#define MAX_KEYWORDS 64
#define MAX_KEYWORD_LEN 32

typedef struct {
    char name[32];               // Numele limbajului (ex: "c")
    char extensions[32];         // Extensii suportate (ex: ".c .h")
    char comment[8];             // Prefix comentariu (ex: "//")
    char keywords[MAX_KEYWORDS][MAX_KEYWORD_LEN];
    int num_keywords;
} SyntaxDef;

SyntaxDef current_syntax;
int syntax_loaded = 0;

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

// Funcție auxiliară pentru a verifica prefixul
int syntax_starts_with(const char* str, const char* pre) {
    while (*pre) {
        if (*pre++ != *str++) return 0;
    }
    return 1;
}

// Încarcă fișierul de configurare în funcție de extensia fișierului deschis
void editor_load_syntax(const char* filename) {
    syntax_loaded = 0;
    current_syntax.num_keywords = 0;
    current_syntax.comment[0] = '\0';

    const char* ext = NULL;
    for (int i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '.') {
            ext = &filename[i];
        }
    }
    if (!ext) return;

    char lang_filename[32] = "syntax_";
    strcat(lang_filename, ext + 1);
    strcat(lang_filename, ".lang");

    uint32_t max_size = 4096;
    uint8_t* buffer = (uint8_t*)nano_malloc(max_size);
    if (!buffer) return;

    int bytes_read = nano_read_file(lang_filename, buffer, max_size - 1);
    if (bytes_read <= 0) {
        nano_free(buffer);
        return;
    }
    buffer[bytes_read] = '\0';

    char line[512];
    int l_idx = 0;
    for (int i = 0; i <= bytes_read; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            line[l_idx] = '\0';
            l_idx = 0;

            if (syntax_starts_with(line, "comment:")) {
                char* val = line + 8;
                while (*val == ' ') val++;
                strcpy(current_syntax.comment, val);
            } 
            else if (syntax_starts_with(line, "keywords:")) {
                char* val = line + 9;
                char kw[MAX_KEYWORD_LEN];
                int kw_i = 0;
                while (*val != '\0') {
                    if (*val == ' ' || *val == '\r') {
                        if (kw_i > 0) {
                            kw[kw_i] = '\0';
                            if (current_syntax.num_keywords < MAX_KEYWORDS) {
                                strcpy(current_syntax.keywords[current_syntax.num_keywords++], kw);
                            }
                            kw_i = 0;
                        }
                    } else {
                        if (kw_i < MAX_KEYWORD_LEN - 1) {
                            kw[kw_i++] = *val;
                        }
                    }
                    val++;
                }
                if (kw_i > 0 && current_syntax.num_keywords < MAX_KEYWORDS) {
                    kw[kw_i] = '\0';
                    strcpy(current_syntax.keywords[current_syntax.num_keywords++], kw);
                }
            }
        } else if (buffer[i] != '\r') {
            if (l_idx < (int)sizeof(line) - 1) {
                line[l_idx++] = buffer[i];
            }
        }
    }

    nano_free(buffer);
    syntax_loaded = 1;
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

    EditorState prev_state = undo_stack[--undo_count];

    for (int i = 0; i < E.num_lines; i++) {
        nano_free(E.lines[i]);
    }
    if (E.lines) nano_free(E.lines);

    E.lines = prev_state.lines;
    E.num_lines = prev_state.num_lines;
    E.cx = prev_state.cx;
    E.cy = prev_state.cy;
    E.modified = 1;

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

    EditorState next_state = redo_stack[--redo_count];

    for (int i = 0; i < E.num_lines; i++) {
        nano_free(E.lines[i]);
    }
    if (E.lines) nano_free(E.lines);

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
    int start_x = E.cx + 1;

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

    char* left_part = (char*)nano_malloc(E.cx + 1);
    memcpy(left_part, line, E.cx);
    left_part[E.cx] = '\0';

    int right_len = len - E.cx;
    char* right_part = (char*)nano_malloc(right_len + 1);
    memcpy(right_part, &line[E.cx], right_len);
    right_part[right_len] = '\0';

    nano_free(E.lines[E.cy]);
    E.lines[E.cy] = left_part;

    editor_insert_row(E.cy + 1, right_part);
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
    
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + render_rows) {
        E.row_offset = E.cy - render_rows + 1;
    }

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
    uint16_t default_attr = ATTR_NORMAL; 
    uint16_t invert_attr  = ATTR_INVERT;

    for (int i = 0; i < total_cells; i++) {
        screen_buffer[i] = default_attr | ' ';
    }

    int render_rows = SCREEN_ROWS - 1;
    
    int in_ml_comment = 0;
    if (syntax_loaded) {
        int comm_len_pre = (int)strlen(current_syntax.comment);
        for (int pre_r = 0; pre_r < E.row_offset && pre_r < E.num_lines; pre_r++) {
            int pre_len = (int)strlen(E.lines[pre_r]);
            int pre_in_str = 0;
            for (int k = 0; k < pre_len; k++) {
                if (!in_ml_comment && E.lines[pre_r][k] == '"') {
                    pre_in_str = !pre_in_str;
                } else if (!pre_in_str) {
                    if (!in_ml_comment && comm_len_pre > 0 &&
                        strncmp(&E.lines[pre_r][k], current_syntax.comment, comm_len_pre) == 0) {
                        break;
                    }
                    if (!in_ml_comment && E.lines[pre_r][k] == '/' && k + 1 < pre_len && E.lines[pre_r][k + 1] == '*') {
                        in_ml_comment = 1;
                        k++;
                    } else if (in_ml_comment && E.lines[pre_r][k] == '*' && k + 1 < pre_len && E.lines[pre_r][k + 1] == '/') {
                        in_ml_comment = 0;
                        k++;
                    }
                }
            }
        }
    }

    for (int i = 0; i < render_rows; i++) {
        int file_row = i + E.row_offset;
        int row_start_idx = i * SCREEN_COLS;
        int col = 0;

        // 1. Afișare numere rânduri dacă sunt activate
        if (E.show_line_numbers) {
            if (file_row < E.num_lines) {
                char num_buf[16];
                sprintf(num_buf, "%4d ", file_row + 1);
                for (int c_idx = 0; num_buf[c_idx] != '\0' && col < SCREEN_COLS; c_idx++) {
                    screen_buffer[row_start_idx + col] = default_attr | (unsigned char)num_buf[c_idx];
                    col++;
                }
            } else {
                for (int c_idx = 0; c_idx < 5 && col < SCREEN_COLS; c_idx++) {
                    screen_buffer[row_start_idx + col] = default_attr | ' ';
                    col++;
                }
            }
        }

        // 2. Randarea conținutului cu Syntax Highlighting
        if (file_row < E.num_lines) {
            int len = (int)strlen(E.lines[file_row]);
            int text_col_start = col; 
            int start_j = (E.col_offset < len) ? E.col_offset : len;

            int in_string = 0;
            int in_angle_header = 0;
            int in_comment = 0;
            int comm_len = (int)strlen(current_syntax.comment);

            // Verificăm dacă linia este o directivă #include
            int is_include_line = 0;
            int first_non_space = 0;
            while (first_non_space < len && (E.lines[file_row][first_non_space] == ' ' || E.lines[file_row][first_non_space] == '\t')) {
                first_non_space++;
            }
            if (first_non_space < len && strncmp(&E.lines[file_row][first_non_space], "#include", 8) == 0) {
                is_include_line = 1;
            }

            if (syntax_loaded) {
                for (int pre_j = 0; pre_j < start_j; pre_j++) {
                    if (!in_ml_comment && E.lines[file_row][pre_j] == '"') {
                        in_string = !in_string;
                    } else if (!in_ml_comment && is_include_line && E.lines[file_row][pre_j] == '<') {
                        in_angle_header = 1;
                    } else if (in_angle_header && E.lines[file_row][pre_j] == '>') {
                        in_angle_header = 0;
                    } else if (!in_string && !in_angle_header) {
                        if (!in_ml_comment && comm_len > 0 && strncmp(&E.lines[file_row][pre_j], current_syntax.comment, comm_len) == 0) {
                            in_comment = 1;
                            break;
                        }
                        if (!in_ml_comment && E.lines[file_row][pre_j] == '/' && pre_j + 1 < len && E.lines[file_row][pre_j + 1] == '*') {
                            in_ml_comment = 1;
                            pre_j++;
                        } else if (in_ml_comment && E.lines[file_row][pre_j] == '*' && pre_j + 1 < len && E.lines[file_row][pre_j + 1] == '/') {
                            in_ml_comment = 0;
                            pre_j++;
                        }
                    }
                }
            }

            int last_rendered_j = start_j - 1;

            for (int j = start_j; j < len && col < SCREEN_COLS; j++) {
                last_rendered_j = j;
                int should_highlight_selection = is_in_selection(file_row, j);
                
                char ch = E.lines[file_row][j];
                uint16_t attr = default_attr;

                if (should_highlight_selection) {
                    attr = invert_attr;
                } else if (syntax_loaded) {
                    if (in_ml_comment) {
                        attr = ATTR_COMMENT;
                        if (ch == '*' && j + 1 < len && E.lines[file_row][j + 1] == '/') {
                            if (col < SCREEN_COLS) screen_buffer[row_start_idx + col++] = ATTR_COMMENT | '*';
                            if (col < SCREEN_COLS) screen_buffer[row_start_idx + col++] = ATTR_COMMENT | '/';
                            in_ml_comment = 0;
                            j++;
                            last_rendered_j = j;
                            continue;
                        }
                    } else if (in_comment) {
                        attr = ATTR_COMMENT;
                    } else if (!in_string && !in_angle_header && ch == '/' && j + 1 < len && E.lines[file_row][j + 1] == '*') {
                        in_ml_comment = 1;
                        if (col < SCREEN_COLS) screen_buffer[row_start_idx + col++] = ATTR_COMMENT | '/';
                        if (col < SCREEN_COLS) screen_buffer[row_start_idx + col++] = ATTR_COMMENT | '*';
                        j++;
                        last_rendered_j = j;
                        continue;
                    } else if (!in_string && !in_angle_header && comm_len > 0 && strncmp(&E.lines[file_row][j], current_syntax.comment, comm_len) == 0) {
                        in_comment = 1;
                        attr = ATTR_COMMENT;
                    } else if (!in_angle_header && ch == '"') {
                        in_string = !in_string;
                        attr = ATTR_STRING;
                    } else if (in_string) {
                        attr = ATTR_STRING;
                    } else if (is_include_line && ch == '<') {
                        in_angle_header = 1;
                        attr = ATTR_STRING;
                    } else if (in_angle_header) {
                        attr = ATTR_STRING;
                        if (ch == '>') {
                            in_angle_header = 0;
                        }
                    } else if (ch >= '0' && ch <= '9' && (j == 0 || !((E.lines[file_row][j-1] >= 'a' && E.lines[file_row][j-1] <= 'z') || (E.lines[file_row][j-1] >= 'A' && E.lines[file_row][j-1] <= 'Z') || E.lines[file_row][j-1] == '_'))) {
                        attr = ATTR_NUMBER;
                    } else {
                        // Verificare Cuvinte Cheie (doar la început de cuvânt!)
                        int is_word_start = (j == 0) || !(
                            (E.lines[file_row][j - 1] >= 'a' && E.lines[file_row][j - 1] <= 'z') ||
                            (E.lines[file_row][j - 1] >= 'A' && E.lines[file_row][j - 1] <= 'Z') ||
                            (E.lines[file_row][j - 1] >= '0' && E.lines[file_row][j - 1] <= '9') ||
                            E.lines[file_row][j - 1] == '_'
                        );

                        if (is_word_start && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')) {
                            char word[32];
                            int w_len = 0;
                            int k = j;
                            while (k < len && ((E.lines[file_row][k] >= 'a' && E.lines[file_row][k] <= 'z') || 
                                               (E.lines[file_row][k] >= 'A' && E.lines[file_row][k] <= 'Z') || 
                                               (E.lines[file_row][k] >= '0' && E.lines[file_row][k] <= '9') || 
                                               E.lines[file_row][k] == '_')) {
                                if (w_len < 31) word[w_len++] = E.lines[file_row][k];
                                k++;
                            }
                            word[w_len] = '\0';

                            int is_kw = 0;
                            for (int kw = 0; kw < current_syntax.num_keywords; kw++) {
                                if (strcmp(word, current_syntax.keywords[kw]) == 0) {
                                    is_kw = 1;
                                    break;
                                }
                            }

                            if (is_kw) {
                                for (int w = 0; w < w_len && col < SCREEN_COLS; w++) {
                                    char kw_ch = word[w];
                                    if (kw_ch == '\t') {
                                        do {
                                            if (col < SCREEN_COLS) {
                                                screen_buffer[row_start_idx + col] = ATTR_KEYWORD | ' ';
                                                col++;
                                            }
                                        } while ((col - text_col_start) % 4 != 0 && col < SCREEN_COLS);
                                    } else {
                                        if (col < SCREEN_COLS) {
                                            screen_buffer[row_start_idx + col] = ATTR_KEYWORD | (unsigned char)kw_ch;
                                            col++;
                                        }
                                    }
                                }
                                j += w_len - 1;
                                last_rendered_j = j;
                                continue;
                            }
                        }
                    }
                }

                // Randarea normală pentru caractere care nu sunt cuvinte cheie
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

            if (syntax_loaded && !in_comment) {
                for (int post_j = last_rendered_j + 1; post_j < len; post_j++) {
                    if (!in_ml_comment && E.lines[file_row][post_j] == '"') {
                        in_string = !in_string;
                    } else if (!in_string) {
                        if (!in_ml_comment && comm_len > 0 && strncmp(&E.lines[file_row][post_j], current_syntax.comment, comm_len) == 0) {
                            break;
                        }
                        if (!in_ml_comment && E.lines[file_row][post_j] == '/' && post_j + 1 < len && E.lines[file_row][post_j + 1] == '*') {
                            in_ml_comment = 1;
                            post_j++;
                        } else if (in_ml_comment && E.lines[file_row][post_j] == '*' && post_j + 1 < len && E.lines[file_row][post_j + 1] == '/') {
                            in_ml_comment = 0;
                            post_j++;
                        }
                    }
                }
            }
        } else {
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
        
        sprintf(status_text, "%s%sFile: %s | %d/%d, Col %d", 
                mode_str, 
                mod_str, 
                E.filename[0] != '\0' ? E.filename : "[No Name]", 
                E.cy + 1, 
                E.num_lines, 
                E.cx + 1 
        );
    }

    int s_len = (int)strlen(status_text);
    for (int i = 0; i < s_len && i < SCREEN_COLS; i++) {
        screen_buffer[status_row_idx + i] = invert_attr | (unsigned char)status_text[i];
    }

    nano_draw_full_screen(screen_buffer);

    // 4. Update poziție cursor
    char cursor_seq[32];
    if (E.mode == MODE_COMMAND) {
        sprintf(cursor_seq, "\033[%d;%dH", SCREEN_ROWS, cmd_input_len + 2);
    } else if (E.mode == MODE_SEARCH) {
        sprintf(cursor_seq, "\033[%d;%dH", SCREEN_ROWS, search_query_len + 2);
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
    
    nano_create_file(E.filename, 0, 0644);

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

    editor_load_syntax(E.filename);
    if (!syntax_loaded) {
        // Fallback de siguranță: dacă nu găsește .lang pe disc, setează manual cuvintele cheie pentru C
        strcpy(current_syntax.comment, "//");
        strcpy(current_syntax.keywords[0], "int");
        strcpy(current_syntax.keywords[1], "return");
        strcpy(current_syntax.keywords[2], "void");
        strcpy(current_syntax.keywords[3], "if");
        strcpy(current_syntax.keywords[4], "else");
        strcpy(current_syntax.keywords[5], "while");
        strcpy(current_syntax.keywords[6], "for");
        current_syntax.num_keywords = 7;
        syntax_loaded = 1;
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
        FILE* f_check = fopen(E.filename, "rb");
        if (f_check) {
            fseek(f_check, 0, SEEK_END);
            long file_size = ftell(f_check);
            rewind(f_check);
            fclose(f_check);

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
            strcpy(E.status_msg, "[New File]");
        }
    }
    
    if (E.num_lines == 0) {
        editor_insert_row(0, "");
    }
    
    E.modified = 0;
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
                undo_recorded_in_session = 0;
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
                    editor_push_undo();
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
                    editor_push_undo();
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
                editor_push_undo();
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
                int spaces = 4 - (E.cx % 4);
                for (int i = 0; i < spaces; i++) {
                    editor_row_insert_char(E.cy, E.cx, ' ');
                    E.cx++;
                }
            } else {
                editor_row_insert_char(E.cy, E.cx, c);
                E.cx++;
            }
        }
        else if (E.mode == MODE_SEARCH) {
            if (c == '\n' || c == '\r') {
                editor_search();
            } else if (c == 8 || c == 127) {
                if (search_query_len > 0) {
                    search_query_len--;
                    search_query[search_query_len] = '\0';
                }
            } else if (c == 27) {
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
                } else if (syntax_starts_with(cmd_input, "w ")) {
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
                } else if (syntax_starts_with(cmd_input, "wq ")) {
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
                else if (strncmp(cmd_input, "e ", 2) == 0) {
                    char* fname = &cmd_input[2];
                    while (*fname == ' ') fname++;
                    if (*fname != '\0') {
                        for (int i = 0; i < E.num_lines; i++) {
                            nano_free(E.lines[i]);
                        }
                        if (E.lines) {
                            nano_free(E.lines);
                            E.lines = NULL;
                        }
                        E.num_lines = 0;
                        E.capacity = 0;

                        strcpy(E.filename, fname);
                        editor_load_syntax(E.filename);
                        E.cx = 0;
                        E.cy = 0;
                        E.row_offset = 0;
                        E.col_offset = 0;

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