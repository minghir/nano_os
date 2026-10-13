#include "nano_libc.h"

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

static uint32_t screen_fb[SCREEN_WIDTH * SCREEN_HEIGHT];

// Mini-font simplu de 5x7 pixeli pentru cifre și câteva litere esențiale (X, Y, B, =)
static const uint8_t font_5x7[][7] = {
    ['0'] = { 0x7E, 0x11, 0x11, 0x11, 0x7E },
    ['1'] = { 0x00, 0x04, 0x7F, 0x00, 0x00 },
    ['2'] = { 0x46, 0x49, 0x49, 0x49, 0x31 },
    ['3'] = { 0x22, 0x41, 0x49, 0x49, 0x7F },
    ['4'] = { 0x0F, 0x08, 0x08, 0x08, 0x7F },
    ['5'] = { 0x79, 0x49, 0x49, 0x49, 0x4F },
    ['6'] = { 0x7E, 0x49, 0x49, 0x49, 0x3E },
    ['7'] = { 0x41, 0x21, 0x11, 0x09, 0x07 },
    ['8'] = { 0x7E, 0x49, 0x49, 0x49, 0x7E },
    ['9'] = { 0x78, 0x48, 0x48, 0x48, 0x7F },
    ['X'] = { 0x41, 0x22, 0x1C, 0x22, 0x41 },
    ['Y'] = { 0x03, 0x04, 0x78, 0x04, 0x03 },
    ['B'] = { 0x7F, 0x49, 0x49, 0x49, 0x36 },
    ['='] = { 0x14, 0x14, 0x14, 0x14, 0x14 },
    [' '] = { 0x00, 0x00, 0x00, 0x00, 0x00 }
};

// Funcție pentru a desena un caracter pe ecran la o anumită poziție (x, y)
void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 0 || c >= 128) c = ' ';
    const uint8_t* glyph = font_5x7[(unsigned char)c];
    if (!glyph) glyph = font_5x7[' '];

    for (int cx = 0; cx < 5; cx++) {
        uint8_t col_data = glyph[cx];
        for (int cy = 0; cy < 7; cy++) {
            if (col_data & (1 << cy)) {
                int px = x + cx;
                int py = y + cy;
                if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                    screen_fb[py * SCREEN_WIDTH + px] = color;
                }
            }
        }
    }
}

// Funcție pentru a desena un șir de caractere (text simplu)
void draw_string(int x, int y, const char* str, uint32_t color) {
    int cursor_x = x;
    while (*str) {
        draw_char(cursor_x, y, *str, color);
        cursor_x += 6; // 5 pixeli lățime caracter + 1 pixel pauză
        str++;
    }
}

// Funcție simplă pentru a converti un număr întreg în text
void int_to_str(int n, char* buf) {
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int i = 0, temp = n, len = 0;
    while (temp > 0) { len++; temp /= 10; }
    buf[len] = '\0';
    while (n > 0) {
        buf[--len] = '0' + (n % 10);
        n /= 10;
    }
}

int main(int argc, char** argv) {
    nano_print("Pornire diagnostic mouse...\n");
    nano_set_desktop_mode(1);
    
    MouseState mouse;
    
    while (1) {
        // 1. Fundal albastru închis
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            screen_fb[i] = 0xFF1A2B3C; 
        }

        // 2. Citim starea mouse-ului prin Syscall
        if (nano_get_mouse_state(&mouse)) {
            // Construim textul pentru coordonate: "X=xxx Y=yyy B=b"
            char buf_x[16], buf_y[16], buf_b[16];
            int_to_str(mouse.x, buf_x);
            int_to_str(mouse.y, buf_y);
            int_to_str(mouse.buttons, buf_b);

            char line[64] = "X=";
            // Concatenare manuală simplă
            // (sau afișăm direct prin funcții separate de draw_string)
            
            // Afișaj direct sus în stânga (10, 10) cu text alb
            draw_string(10, 10, "X=", 0xFFFFFFFF);
            draw_string(28, 10, buf_x, 0xFFFFFFFF);

            draw_string(80, 10, "Y=", 0xFFFFFFFF);
            draw_string(98, 10, buf_y, 0xFFFFFFFF);

            draw_string(150, 10, "BTN=", 0xFFFFFFFF);
            draw_string(186, 10, buf_b, 0xFFFFFFFF);
        } else {
            draw_string(10, 10, "EROARE CITIRE MOUSE!", 0xFFFF0000);
        }

        // 3. Trimitem totul pe ecran
        nano_gfx_present(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screen_fb);
        
        if (nano_has_char()) {
            if (nano_read_char() == 'q') break;
        }

        nano_sleep(30); 
    }

    nano_set_desktop_mode(0);
    nano_print("Diagnostic închis.\n");
    return 0;
}