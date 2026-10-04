#include "io.h"
#include "idt.h"
#include "tty.h"
#include "process.h"
#include "sys.h"
#include "mouse.h"
#include "gfx/gfx_term.h"

extern void video_swap_buffers();
// ==========================================
// VARIABILE DE CONTROL VIDEO (HARDWARE ABSTRACTION)
// ==========================================
int is_gfx_mode = 0;        // 0 = VGA Text (0xB8000), 1 = Framebuffer
int term_cols = 80;         // Default pentru mod text
int term_rows = 25;         // Default pentru mod text

uint16_t current_vga_attr = FONT_COLOR;
uint16_t* physical_vga = (uint16_t*)0xB8000;
int cursor = 0;
volatile uint8_t keyboard_running = 1;

static int cursor_visible = 1;
static uint32_t blink_counter = 0;
#define BLINK_INTERVAL_MS 400


extern void gterm_draw_cursor_box(int x, int y, uint32_t color);
extern int mouse_x; // Coordonata X curentă a mouse-ului
extern int mouse_y; // Coordonata Y curentă a mouse-ului

// Funcție pentru a activa modul grafic din kernel_main
void io_set_graphical_mode(int cols, int rows) {
    is_gfx_mode = 1;
    term_cols = cols;
    term_rows = rows;
}



// Convertor VGA (4-bit) -> ARGB (32-bit)
static uint32_t vga_to_argb(uint8_t vga_color) {
    static const uint32_t palette[16] = {
        0x00000000, 0x000000AA, 0x0000AA00, 0x0000AAAA,
        0x00AA0000, 0x00AA00AA, 0x00AA5500, 0x00AAAAAA,
        0x00555555, 0x005555FF, 0x0055FF55, 0x0055FFFF,
        0x00FF5555, 0x00FF55FF, 0x00FFFF55, 0x00FFFFFF
    };
    return palette[vga_color & 0x0F];
}

//#define FONT_COLOR (0x0F << 8)

// Atributul VGA curent (folosit pentru text normal sau evidențiere / Reverse Video)
//uint16_t current_vga_attr = FONT_COLOR; // Păstrăm culoarea verde setată de tine

//uint16_t* VGA = (uint16_t*)0xB8000;
//uint16_t* physical_vga = (uint16_t*)0xB8000;

//int cursor = 0;
//volatile uint8_t keyboard_running = 1;
static volatile uint8_t keyboard_queue[256];
static volatile uint8_t keyboard_queue_read = 0;
static volatile uint8_t keyboard_queue_write = 0;



// ==========================================
// ABSTRACȚII PENTRU DESENARE (RUTEAZĂ CĂTRE TEXT SAU GRAFIC)
// ==========================================


static void draw_cursor_gfx(int tty_id, int show) {
    if (tty_id != active_tty || !is_gfx_mode) return;
    int pos = ttys[tty_id].cursor;
    int col = pos % term_cols;
    int row = pos / term_cols;
    
    uint16_t cell = ttys[tty_id].screen_buffer[pos];
    char c = show ? '_' : (cell & 0xFF);
    if (!show && c == 0) c = ' ';
    
    uint8_t fg_vga = (cell >> 8) & 0x0F;
    gterm_draw_char(col, row, c, vga_to_argb(fg_vga), 0x00000000);
	
	video_swap_buffers();
}


void cursor_blink_tick() {
    if (!is_gfx_mode) return; // Doar în mod grafic
    
    blink_counter++;
    if (blink_counter >= BLINK_INTERVAL_MS) {
        blink_counter = 0;
        cursor_visible = !cursor_visible; // Inversăm starea (aprins / stins)
        draw_cursor_gfx(active_tty, cursor_visible);
    }
}


void cursor_update() {
    if (is_gfx_mode) {
        cursor_visible = 1;     // Forțăm cursorul să fie vizibil când tastăm/scriem
        blink_counter = 0;      // Resetăm contorul de clipire
        draw_cursor_gfx(active_tty, 1);
    } else {
        uint16_t position = (uint16_t)ttys[active_tty].cursor;
        outb(0x3D4, 0x0F);
        outb(0x3D5, position & 0xFF);
        outb(0x3D4, 0x0E);
        outb(0x3D5, position >> 8);
    }
}

void cursor_init() {
    if (!is_gfx_mode) {
        outb(0x3D4, 0x0A); outb(0x3D5, 0x00);
        outb(0x3D4, 0x0B); outb(0x3D5, 0x0F);
    }
    cursor_update();
}
/*
void gfx_redraw_tty(int tty_id) {
    if (tty_id != active_tty) return;
    if (is_gfx_mode) {
        for (int i = 0; i < term_cols * term_rows; i++) {
            int col = i % term_cols;
            int row = i / term_cols;
            uint16_t cell = ttys[tty_id].screen_buffer[i];
            char c = (cell & 0xFF) ? (cell & 0xFF) : ' ';
            uint8_t fg = (cell >> 8) & 0x0F;
            gterm_draw_char(col, row, c, vga_to_argb(fg), 0x00000000);
        }
    } else {
        for (int i = 0; i < term_cols * term_rows; i++) {
            physical_vga[i] = ttys[tty_id].screen_buffer[i];
        }
    }
    cursor_update();
}
*/
static uint16_t last_screen_buffer[MAX_TTYS][256 * 128];

void gfx_redraw_tty(int tty_id) {
    if (tty_id != active_tty) return;
    if (is_gfx_mode) {
        for (int i = 0; i < term_cols * term_rows; i++) {
            int col = i % term_cols;
            int row = i / term_cols;
            uint16_t cell = ttys[tty_id].screen_buffer[i];
            char c = (cell & 0xFF) ? (cell & 0xFF) : ' ';
            
            // Extragem culoarea textului (primii 4 biți din octetul de atribut)
            uint8_t fg = (cell >> 8) & 0x0F;
            // Extragem culoarea fundalului (următorii 4 biți)
            uint8_t bg = (cell >> 12) & 0x0F; 
            
            // Desenăm folosind ambele culori transformate!
            gterm_draw_char(col, row, c, vga_to_argb(fg), vga_to_argb(bg));
        }
		
		gterm_draw_cursor_box(mouse_x, mouse_y, 0x00FFFFFF);
		video_swap_buffers();
    } else {
        for (int i = 0; i < term_cols * term_rows; i++) {
            physical_vga[i] = ttys[tty_id].screen_buffer[i];
        }
    }
    cursor_update();
}


// În structura ta TTY sau ca array static în fișierul tău de randare TTY:
// static uint16_t last_screen_buffer[MAX_TERM_HEIGHT][MAX_TERM_WIDTH]; // sau alocat dinamic
// Cache static separat pentru fiecare TTY (presupunând dimensiunea maximă de 256x128 caractere)

/*
void gfx_redraw_tty(int tty_id) {
    if (tty_id != active_tty) return;
    
    if (is_gfx_mode) {
        int changed_cells = 0;
        
        for (int i = 0; i < term_cols * term_rows; i++) {
            uint16_t new_cell = ttys[tty_id].screen_buffer[i];
            
            // Verificăm folosind array-ul static local, nu structura TTY
            if (new_cell != last_screen_buffer[tty_id][i]) {
                int col = i % term_cols;
                int row = i / term_cols;
                
                char c = (new_cell & 0xFF) ? (new_cell & 0xFF) : ' ';
                uint8_t fg = (new_cell >> 8) & 0x0F;
                uint8_t bg = (new_cell >> 12) & 0x0F; 
                
                // Randonăm fontul 8x16 DOAR pentru celula modificată!
                gterm_draw_char(col, row, c, vga_to_argb(fg), vga_to_argb(bg));
                
                // Salvăm în cache-ul static
                last_screen_buffer[tty_id][i] = new_cell;
                changed_cells++;
            }
        }
        
        // Facem swap doar dacă s-a modificat efectiv ceva pe ecran
        if (changed_cells > 0) {
            video_swap_buffers();
        }
        
    } else {
        for (int i = 0; i < term_cols * term_rows; i++) {
            physical_vga[i] = ttys[tty_id].screen_buffer[i];
        }
    }
    cursor_update();
}
*/


static void scroll_screen_tty(int target_tty) {
    for (int i = 0; i < term_cols * (term_rows - 1); i++) {
        ttys[target_tty].screen_buffer[i] = ttys[target_tty].screen_buffer[i + term_cols];
    }
    for (int i = term_cols * (term_rows - 1); i < term_cols * term_rows; i++) {
        ttys[target_tty].screen_buffer[i] = ttys[target_tty].current_vga_attr | ' ';
    }
    if (target_tty == active_tty) {
        gfx_redraw_tty(target_tty);
    }
}

static void print_to_tty(const char* s, int target_tty) {
    int cursor = ttys[target_tty].cursor;
    uint16_t current_vga_attr = ttys[target_tty].current_vga_attr;

    if (is_gfx_mode) draw_cursor_gfx(target_tty, 0); // Stergem cursor gfx

    while (*s) {
        // --- Secvențe ANSI (Neschimbate, funcționează perfect) ---
        if (*s == '\033' && s[1] == '[') {
            s += 2; 
            if (*s == 'D') {
                s++; if (cursor > 0) cursor--;
                ttys[target_tty].cursor = cursor;
                if (!is_gfx_mode && target_tty == active_tty) cursor_update();
                continue;
            }
            if (*s == 'C') {
                s++; if (cursor < term_cols * term_rows - 1) cursor++;
                ttys[target_tty].cursor = cursor;
                if (!is_gfx_mode && target_tty == active_tty) cursor_update();
                continue;
            }
            if (*s >= '0' && *s <= '9') {
                int val = 0; const char* temp = s;
                while (*temp >= '0' && *temp <= '9') { val = val * 10 + (*temp - '0'); temp++; }
                if (*temp == 'm') {
                    s = temp + 1;
                    if (val == 7) current_vga_attr = 0x7000;
                    else if (val == 0) current_vga_attr = FONT_COLOR;
                    ttys[target_tty].current_vga_attr = current_vga_attr;
                    continue;
                }
            }
            int r = 0; while (*s >= '0' && *s <= '9') { r = r * 10 + (*s - '0'); s++; }
            if (*s == ';') {
                s++; int c = 0; while (*s >= '0' && *s <= '9') { c = c * 10 + (*s - '0'); s++; }
                if (*s == 'H') {
                    s++;
                    int row = r - 1; int col = c - 1;
                    if (row < 0) row = 0; if (row >= term_rows) row = term_rows - 1;
                    if (col < 0) col = 0; if (col >= term_cols) col = term_cols - 1;
                    cursor = row * term_cols + col;
                    ttys[target_tty].cursor = cursor;
                    if (!is_gfx_mode && target_tty == active_tty) cursor_update();
                    continue;
                }
            }
            while (*s && *s != 'm' && *s != 'H' && *s != 'D' && *s != 'C') s++;
            if (*s) s++;
            continue;
        }

        char character = *s++;

        if (character == '\n') {
            cursor = (cursor / term_cols + 1) * term_cols;
            if (cursor >= term_cols * term_rows) {
                scroll_screen_tty(target_tty);
                cursor = term_cols * (term_rows - 1);
            }
            ttys[target_tty].cursor = cursor;
            continue;
        }

        if (character == '\b') {
            if (cursor > 0) {
                cursor--;
                ttys[target_tty].screen_buffer[cursor] = current_vga_attr | ' ';
                if (target_tty == active_tty) {
                    if (is_gfx_mode) {
                        gterm_draw_char(cursor % term_cols, cursor / term_cols, ' ', vga_to_argb(current_vga_attr >> 8), 0x00000000);
                    } else {
                        physical_vga[cursor] = current_vga_attr | ' ';
                    }
                }
            }
            ttys[target_tty].cursor = cursor;
            continue;
        }

        // --- SCRIERE EFECTIVĂ DUALĂ ---
        ttys[target_tty].screen_buffer[cursor] = current_vga_attr | character;
        if (target_tty == active_tty) {
            if (is_gfx_mode) {
                gterm_draw_char(cursor % term_cols, cursor / term_cols, character, vga_to_argb(current_vga_attr >> 8), 0x00000000);
            } else {
                physical_vga[cursor] = current_vga_attr | character;
            }
        }
        cursor++;
        
        if (cursor >= term_cols * term_rows) {
            scroll_screen_tty(target_tty);
            cursor = term_cols * (term_rows - 1);
        }
        ttys[target_tty].cursor = cursor;
    }
    
    if (target_tty == active_tty) cursor_update();
}

void print(const char* s) {
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    print_to_tty(s, target_tty);
}

void print_syslog(const char* s) {
    print_to_tty(s, 0);
}

void newline() {
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    int cursor = ttys[target_tty].cursor;
    
    if (is_gfx_mode) draw_cursor_gfx(target_tty, 0);
    
    cursor = (cursor / term_cols + 1) * term_cols;
    if (cursor >= term_cols * term_rows) {
        scroll_screen_tty(target_tty);
        cursor = term_cols * (term_rows - 1);
    }
    ttys[target_tty].cursor = cursor;
    if (target_tty == active_tty) cursor_update();
}

void clear_screen() {
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    for (int index = 0; index < term_cols * term_rows; index++) {
        ttys[target_tty].screen_buffer[index] = ttys[target_tty].current_vga_attr | ' ';
    }
    ttys[target_tty].cursor = 0;
    
    if (target_tty == active_tty) {
        if (is_gfx_mode) {
            gterm_clear();
        } else {
            for (int i = 0; i < term_cols * term_rows; i++) physical_vga[i] = ttys[target_tty].screen_buffer[i];
        }
        cursor_update();
    }
}

void set_cursor_shape(int style) {
    // style 0 = bloc plin, style 1 = underline
    outb(0x3D4, 0x0A);
    if (style == 1) {
        outb(0x3D5, 12); // Începe de la rândul 12 din 15 (underline)
    } else {
        outb(0x3D5, 0);  // Începe de la rândul 0 (bloc)
    }
    
    outb(0x3D4, 0x0B);
    outb(0x3D5, 15);     // Se termină la rândul 15
}

// Pune asta în src/io.c (și șterge-o pe cea veche)
static void scroll_screen() {
    uint16_t* physical_vga = (uint16_t*)0xB8000;

    // Mutăm caracterele cu o linie mai sus în bufferul TTY-ului activ
    for (int i = 0; i < 80 * 24; i++) {
        ttys[active_tty].screen_buffer[i] = ttys[active_tty].screen_buffer[i + 80];
    }
    
    // Curățăm complet ultima linie (linia 24) în buffer
    for (int i = 80 * 24; i < 80 * 25; i++) {
        ttys[active_tty].screen_buffer[i] = ttys[active_tty].current_vga_attr | ' ';
    }
    
    // Oglindim imediat pe ecranul fizic
    for (int i = 0; i < 80 * 25; i++) {
        physical_vga[i] = ttys[active_tty].screen_buffer[i];
    }
}

void pic_remap() {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

/*
void pic_enable_irq(int irq) {
    uint8_t mask = inb(0x21);
    mask &= ~(1 << irq);
    outb(0x21, mask);
}
*/

void pic_enable_irq(int irq) {
    if (irq < 8) {
        uint8_t mask = inb(0x21);
        mask &= ~(1 << irq);
        outb(0x21, mask); // Deblochează pe Master (inclusiv IRQ 1 pentru tastatură!)
    } else {
        uint8_t slave_mask = inb(0xA1);
        slave_mask &= ~(1 << (irq - 8));
        outb(0xA1, slave_mask); // Deblochează pe Slave (inclusiv IRQ 12 pentru mouse!)
        
        // FOARTE IMPORTANT: Asigură-te că linia 2 de pe master (care leagă slave-ul) este deschisă!
        uint8_t master_mask = inb(0x21);
        master_mask &= ~(1 << 2); 
        outb(0x21, master_mask);
    }
}

static const char keyboard_map[128] = {
    [0x01] = 27,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x',
    [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' '
};

static void keyboard_queue_push(uint8_t character) {
    // Tasta tastată merge ÎNTOTDEAUNA în coada TTY-ului la care ne uităm acum (active_tty)
    int target_tty = active_tty;

    uint8_t next = ttys[target_tty].keyboard_queue_write + 1;
    if (next == ttys[target_tty].keyboard_queue_read) {
        return;
    }
    ttys[target_tty].keyboard_queue[ttys[target_tty].keyboard_queue_write] = character;
    ttys[target_tty].keyboard_queue_write = next;
}

// Când shell-ul vrea o tastă, va citi din TTY-ul CĂRUIA ÎI APARȚINE!
int keyboard_read_char() {
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    
    // Cât timp coada acestui TTY specific este goală, procesul cedează controlul prin hlt
    while (ttys[target_tty].keyboard_queue_read == ttys[target_tty].keyboard_queue_write) {
        __asm__ volatile("sti; hlt");
    }
    
    uint8_t character = ttys[target_tty].keyboard_queue[ttys[target_tty].keyboard_queue_read];
    ttys[target_tty].keyboard_queue_read++;
    return (int)character;
}

void keyboard_read_line(char* buffer, uint32_t max_length) {
    uint32_t index = 0;
    
    while (1) {
        char c = keyboard_read_char();
        
        // Dacă nu s-a apăsat nimic, așteptăm AICI (funcția asta e folosită doar intern)
        if (c == 0) {
            __asm__ volatile("sti; hlt");
            continue;
        }
        
        // Dacă e Enter, închidem textul și ieșim din buclă
        if (c == '\n' || c == '\r') {
            buffer[index] = '\0';
            newline();
            break;
        } 
        // Dacă e Backspace, ștergem ultima literă (dacă există)
        else if (c == '\b') {
            if (index > 0) {
                index--;
                print("\b"); // Ștergem de pe ecran
            }
        } 
        // Acceptăm doar text real
        else if (c >= 32 && c <= 126) {
            if (index < max_length - 1) {
                buffer[index++] = c;
                char temp_str[2] = {c, 0};
                print(temp_str);
            }
        }
    }
}

void keyboard_irq() {
    uint8_t scancode = inb(0x60);
    static uint8_t ctrl_pressed = 0;
    static uint8_t shift_pressed = 0;
    static uint8_t alt_pressed = 0;
    static uint8_t is_extended = 0; 

    if (scancode == 0xE0) {
        is_extended = 1;
        return;
    }

    if (is_extended) {
        is_extended = 0;
        
        // Dacă e Right Ctrl (Eliberare)
        if (scancode == (0x1D | 0x80)) { ctrl_pressed = 0; return; }
        // Dacă e Right Alt (Eliberare)
        if (scancode == (0x38 | 0x80)) { alt_pressed = 0; return; }
        
        // Orice altă eliberare extinsă
        if (scancode & 0x80) return;

        // Dacă e Right Ctrl (Apăsare)
        if (scancode == 0x1D) { ctrl_pressed = 1; return; }
        // Dacă e Right Alt (Apăsare)
        if (scancode == 0x38) { alt_pressed = 1; return; }

        if (scancode == 0x48) { keyboard_queue_push(128); return; } // Sus
        if (scancode == 0x50) { keyboard_queue_push(129); return; } // Jos
        if (scancode == 0x4D) { keyboard_queue_push(130); return; } // Dreapta
        if (scancode == 0x4B) { keyboard_queue_push(131); return; } // Stânga
        if (scancode == 0x53) { keyboard_queue_push(132); return; } // Delete
        if (scancode == 0x47) { keyboard_queue_push(133); return; } // Home
        if (scancode == 0x49) { keyboard_queue_push(134); return; } // PgUp
        if (scancode == 0x4F) { keyboard_queue_push(135); return; } // End
        if (scancode == 0x51) { keyboard_queue_push(136); return; } // PgDn
        if (scancode == 0x52) { keyboard_queue_push(137); return; } // Insert
        return;
    }

    // Eliberarea tastelor normale (inclusiv Left Ctrl)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x1D) ctrl_pressed = 0;
        else if (released == 0x2A || released == 0x36) shift_pressed = 0;
        else if (released == 0x38) alt_pressed = 0;
        return;
    }

    // Apăsarea tastelor modificatoare normale
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x38) { alt_pressed = 1; return; }

    // ==========================================================
    // SHORTCUT-URI WORKSPACE
    // ==========================================================
    // Apăsarea tastelor modificatoare normale
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x38) { alt_pressed = 1; return; }

    // ==========================================================
    // SHORTCUT-URI WORKSPACE (Fără Ctrl, doar tastele F directe!)
    // ==========================================================
    // Apasă direct F1, F2, F3 sau F4 pentru a schimba workspace-ul!
    if (scancode == 0x3B) { switch_tty(0); return; } // F1 -> TTY 0
    if (scancode == 0x3C) { switch_tty(1); return; } // F2 -> TTY 1
    if (scancode == 0x3D) { switch_tty(2); return; } // F3 -> TTY 2
    if (scancode == 0x3E) { switch_tty(3); return; } // F4 -> TTY 3
    
    // Aici păstrăm Ctrl+C (doar ăsta depinde de ctrl_pressed)
    if (ctrl_pressed && scancode == 0x2E) {
		uint32_t fg_pid = ttys[active_tty].foreground_pid;
        
        // Dacă există un proces valid în foreground pe acest TTY
        if (fg_pid != 0) {
            kill_process_by_pid(fg_pid);
            
            // Opțional: trimitem și un newline pe ecran ca să arate curat
            print("^C\n");
        }
        keyboard_queue_push(3);
        return;
    }
    // ==========================================================

    if (alt_pressed && scancode == 0x2E) {
        clear_screen();
        return;
    }

    // Limitarea mapei de caractere
    if (scancode >= sizeof(keyboard_map)) return;

    if (alt_pressed && scancode == 0x2E) {
        clear_screen();
        return;
    }

    // Limitarea mapei de caractere
    if (scancode >= sizeof(keyboard_map)) return;
    
    char character = keyboard_map[scancode];
    if (character == 0) return;

    if (shift_pressed) {
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        else if (character == '-') character = '_';
        else if (character == '1') character = '!';
        else if (character == '2') character = '@';
        else if (character == '3') character = '#';
        else if (character == '4') character = '$';
        else if (character == '5') character = '%';
        else if (character == '6') character = '^';
        else if (character == '7') character = '&';
        else if (character == '8') character = '*';
        else if (character == '9') character = '(';
        else if (character == '0') character = ')';
        else if (character == '=') character = '+';
        else if (character == '[') character = '{';
        else if (character == ']') character = '}';
        else if (character == '\\') character = '|';
        else if (character == ';') character = ':';
        else if (character == '\'') character = '"';
        else if (character == ',') character = '<';
        else if (character == '.') character = '>';
        else if (character == '/') character = '?';
    }
    
    keyboard_queue_push((uint8_t)character);
}

void keyboard_stop_message() {
    print("Ctrl+C received. Keyboard loop stopped.");
    newline();
}

void print_at(int row, int col, const char* s) {
    uint16_t* vga = (uint16_t*)0xB8000;
    int index = row * 80 + col;
    
    while (*s) {
        vga[index++] = FONT_COLOR | *s++;
    }
}


void keyboard_flush() {
    ttys[active_tty].keyboard_queue_read = 0;
    ttys[active_tty].keyboard_queue_write = 0;
}


int keyboard_has_data() {
    return (ttys[active_tty].keyboard_queue_read != ttys[active_tty].keyboard_queue_write);
}