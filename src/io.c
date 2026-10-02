#include "io.h"
#include "idt.h"
#include "tty.h"
#include "process.h"

//#define FONT_COLOR (0x0F << 8)

// Atributul VGA curent (folosit pentru text normal sau evidențiere / Reverse Video)
uint16_t current_vga_attr = FONT_COLOR; // Păstrăm culoarea verde setată de tine

//uint16_t* VGA = (uint16_t*)0xB8000;
uint16_t* physical_vga = (uint16_t*)0xB8000;

int cursor = 0;
volatile uint8_t keyboard_running = 1;
static volatile uint8_t keyboard_queue[256];
static volatile uint8_t keyboard_queue_read = 0;
static volatile uint8_t keyboard_queue_write = 0;

// Actualizăm cursor_update să citească din TTY-ul activ
void cursor_update() {
    uint16_t position = (uint16_t)ttys[active_tty].cursor;

    outb(0x3D4, 0x0F);
    outb(0x3D5, position & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

void cursor_init() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x00);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
    cursor_update();
}

// Scroll a fost modificat să facă scroll DOAR pe bufferul TTY-ului țintă
static void scroll_screen_tty(int target_tty) {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 24; i++) {
        ttys[target_tty].screen_buffer[i] = ttys[target_tty].screen_buffer[i + 80];
    }
    for (int i = 80 * 24; i < 80 * 25; i++) {
        ttys[target_tty].screen_buffer[i] = ttys[target_tty].current_vga_attr | ' ';
    }
    // Oglindim fizic DOAR dacă TTY-ul țintă e cel pe care îl vizionăm
    if (target_tty == active_tty) {
        for (int i = 0; i < 80 * 25; i++) {
            physical_vga[i] = ttys[target_tty].screen_buffer[i];
        }
    }
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

/*
void print(const char* s) {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    
    // Preluăm starea locală din TTY-ul activ
    int cursor = ttys[active_tty].cursor;
    uint16_t current_vga_attr = ttys[active_tty].current_vga_attr;

    while (*s) {
        // Interceptăm secvențele ANSI
        if (*s == '\033' && s[1] == '[') {
            s += 2; // Trecem peste "\033["
            
            // --- Suport pentru Cursor Left (\033[D) ---
            if (*s == 'D') {
                s++;
                if (cursor > 0) cursor--;
                ttys[active_tty].cursor = cursor;
                cursor_update();
                continue;
            }
            // --- Suport pentru Cursor Right (\033[C) ---
            if (*s == 'C') {
                s++;
                if (cursor < 80 * 25 - 1) cursor++;
                ttys[active_tty].cursor = cursor;
                cursor_update();
                continue;
            }

            // Verificăm dacă este comandă de stil (ex: 7m sau 0m)
            if (*s >= '0' && *s <= '9') {
                int val = 0;
                const char* temp = s;
                while (*temp >= '0' && *temp <= '9') {
                    val = val * 10 + (*temp - '0');
                    temp++;
                }
                if (*temp == 'm') {
                    s = temp + 1;
                    if (val == 7) {
                        current_vga_attr = 0x7000;
                    } else if (val == 0) {
                        current_vga_attr = FONT_COLOR;
                    }
                    ttys[active_tty].current_vga_attr = current_vga_attr;
                    continue;
                }
            }
            
            // Comanda de poziționare a cursorului: \033[Row;ColH
            int r = 0;
            while (*s >= '0' && *s <= '9') {
                r = r * 10 + (*s - '0');
                s++;
            }
            
            if (*s == ';') {
                s++;
                int c = 0;
                while (*s >= '0' && *s <= '9') {
                    c = c * 10 + (*s - '0');
                    s++;
                }
                
                if (*s == 'H') {
                    s++;
                    int row = r - 1;
                    int col = c - 1;
                    if (row < 0) row = 0; if (row > 24) row = 24;
                    if (col < 0) col = 0; if (col > 79) col = 79;
                    cursor = row * 80 + col;
                    ttys[active_tty].cursor = cursor;
                    cursor_update();
                    continue;
                }
            }
            
            while (*s && *s != 'm' && *s != 'H' && *s != 'D' && *s != 'C') {
                s++;
            }
            if (*s) s++;
            continue;
        }

        char character = *s++;

        if (character == '\n') {
            cursor = (cursor / 80 + 1) * 80;
            if (cursor >= 80 * 25) {
                scroll_screen();
                cursor = 80 * 24;
            }
            ttys[active_tty].cursor = cursor;
            continue;
        }

        if (character == '\b') {
            if (cursor > 0) {
                cursor--;
                ttys[active_tty].screen_buffer[cursor] = current_vga_attr | ' ';
                physical_vga[cursor] = current_vga_attr | ' ';
            }
            ttys[active_tty].cursor = cursor;
            continue;
        }

        // SCRIERE DUALĂ: Salvăm în bufferul TTY-ului și afișăm pe ecranul fizic
        ttys[active_tty].screen_buffer[cursor] = current_vga_attr | character;
        physical_vga[cursor] = current_vga_attr | character;
        cursor++;
        
        if (cursor >= 80 * 25) {
            scroll_screen();
            cursor = 80 * 24;
        }
        
        // Salvăm cursorul înapoi în structură
        ttys[active_tty].cursor = cursor;
    }
    cursor_update();
}
*/
void print(const char* s) {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    
    // AICI E MAGIA: Aflăm în ce TTY trebuie să scriem. Dacă e kernelul (NULL), scriem în active_tty.
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    
    int cursor = ttys[target_tty].cursor;
    uint16_t current_vga_attr = ttys[target_tty].current_vga_attr;

    while (*s) {
        if (*s == '\033' && s[1] == '[') {
            s += 2; 
            if (*s == 'D') {
                s++;
                if (cursor > 0) cursor--;
                ttys[target_tty].cursor = cursor;
                if (target_tty == active_tty) cursor_update();
                continue;
            }
            if (*s == 'C') {
                s++;
                if (cursor < 80 * 25 - 1) cursor++;
                ttys[target_tty].cursor = cursor;
                if (target_tty == active_tty) cursor_update();
                continue;
            }
            if (*s >= '0' && *s <= '9') {
                int val = 0;
                const char* temp = s;
                while (*temp >= '0' && *temp <= '9') {
                    val = val * 10 + (*temp - '0');
                    temp++;
                }
                if (*temp == 'm') {
                    s = temp + 1;
                    if (val == 7) current_vga_attr = 0x7000;
                    else if (val == 0) current_vga_attr = FONT_COLOR;
                    ttys[target_tty].current_vga_attr = current_vga_attr;
                    continue;
                }
            }
            int r = 0;
            while (*s >= '0' && *s <= '9') { r = r * 10 + (*s - '0'); s++; }
            if (*s == ';') {
                s++;
                int c = 0;
                while (*s >= '0' && *s <= '9') { c = c * 10 + (*s - '0'); s++; }
                if (*s == 'H') {
                    s++;
                    int row = r - 1; int col = c - 1;
                    if (row < 0) row = 0; if (row > 24) row = 24;
                    if (col < 0) col = 0; if (col > 79) col = 79;
                    cursor = row * 80 + col;
                    ttys[target_tty].cursor = cursor;
                    if (target_tty == active_tty) cursor_update();
                    continue;
                }
            }
            while (*s && *s != 'm' && *s != 'H' && *s != 'D' && *s != 'C') s++;
            if (*s) s++;
            continue;
        }

        char character = *s++;

        if (character == '\n') {
            cursor = (cursor / 80 + 1) * 80;
            if (cursor >= 80 * 25) {
                scroll_screen_tty(target_tty);
                cursor = 80 * 24;
            }
            ttys[target_tty].cursor = cursor;
            continue;
        }

        if (character == '\b') {
            if (cursor > 0) {
                cursor--;
                ttys[target_tty].screen_buffer[cursor] = current_vga_attr | ' ';
                if (target_tty == active_tty) physical_vga[cursor] = current_vga_attr | ' ';
            }
            ttys[target_tty].cursor = cursor;
            continue;
        }

        ttys[target_tty].screen_buffer[cursor] = current_vga_attr | character;
        if (target_tty == active_tty) physical_vga[cursor] = current_vga_attr | character;
        cursor++;
        
        if (cursor >= 80 * 25) {
            scroll_screen_tty(target_tty);
            cursor = 80 * 24;
        }
        ttys[target_tty].cursor = cursor;
    }
    // Update hardware cursor only if we are printing to the active screen
    if (target_tty == active_tty) cursor_update();
}

// --- 3. Newline modificat pentru a face scroll ---
/*
void newline() {
    int cursor = ttys[active_tty].cursor;
    cursor = (cursor / 80 + 1) * 80;
    
    if (cursor >= 80 * 25) {
        scroll_screen();
        cursor = 80 * 24;
    }
    ttys[active_tty].cursor = cursor;
    cursor_update();
}
*/

void newline() {
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    int cursor = ttys[target_tty].cursor;
    cursor = (cursor / 80 + 1) * 80;
    
    if (cursor >= 80 * 25) {
        scroll_screen_tty(target_tty);
        cursor = 80 * 24;
    }
    ttys[target_tty].cursor = cursor;
    if (target_tty == active_tty) cursor_update();
}

/*
void clear_screen() {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    for (int index = 0; index < 80 * 25; index++) {
        ttys[active_tty].screen_buffer[index] = ttys[active_tty].current_vga_attr | ' ';
        physical_vga[index] = ttys[active_tty].screen_buffer[index];
    }
    ttys[active_tty].cursor = 0;
    cursor_update();
}
*/

void clear_screen() {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
    
    for (int index = 0; index < 80 * 25; index++) {
        ttys[target_tty].screen_buffer[index] = ttys[target_tty].current_vga_attr | ' ';
        if (target_tty == active_tty) {
            physical_vga[index] = ttys[target_tty].screen_buffer[index];
        }
    }
    ttys[target_tty].cursor = 0;
    if (target_tty == active_tty) cursor_update();
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

void pic_enable_irq(int irq) {
    uint8_t mask = inb(0x21);
    mask &= ~(1 << irq);
    outb(0x21, mask);
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
/*
static void keyboard_queue_push(uint8_t character) {
    uint8_t next = ttys[active_tty].keyboard_queue_write + 1;
    if (next == ttys[active_tty].keyboard_queue_read) {
        return;
    }
    ttys[active_tty].keyboard_queue[ttys[active_tty].keyboard_queue_write] = character;
    ttys[active_tty].keyboard_queue_write = next;
}
*/

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

/*
int keyboard_read_char() {
    while (ttys[active_tty].keyboard_queue_read == ttys[active_tty].keyboard_queue_write) {
        __asm__ volatile("sti; hlt");
    }
    uint8_t character = ttys[active_tty].keyboard_queue[ttys[active_tty].keyboard_queue_read];
    ttys[active_tty].keyboard_queue_read++;
    return (int)character;
}
*/

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
