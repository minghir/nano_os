#include "nano_libc.h"

#define WIN_WIDTH  640
#define WIN_HEIGHT 400
#define TERM_COLS  80
#define TERM_ROWS  25

static uint32_t window_fb[WIN_WIDTH * WIN_HEIGHT];
static uint16_t tty_buffer[TERM_COLS * TERM_ROWS];

// Funcție ajutătoare pentru culori (copiată din kernel)
static uint32_t vga_to_argb(uint8_t vga_color) {
    static const uint32_t palette[16] = {
        0xFF000000, 0xFFAA0000, 0xFF00AA00, 0xFFAAAA00,
        0xFF0000AA, 0xFFAA00AA, 0xFF0055AA, 0xFFAAAAAA,
        0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
        0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF
    };
    return palette[vga_color & 0x0F];
}

int main() {
    // 1. Trecem în modul ferestre
    nano_set_desktop_mode(1);
    
    int win_x = 100;
    int win_y = 100;

    while (1) {
        // 2. Cerem kernelului bufferul de text al TTY-ului 0 (sau active_tty)
        nano_get_tty_buffer(0, tty_buffer);
        
        // 3. Desenăm fereastra (Fundal negru pentru terminal)
        for (int i = 0; i < WIN_WIDTH * WIN_HEIGHT; i++) {
            window_fb[i] = 0xFF000000; // Negru opac
        }
        
        // Bara de titlu a ferestrei
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < WIN_WIDTH; x++) {
                window_fb[y * WIN_WIDTH + x] = 0xFF444444; // Gri
            }
        }
        // Aici poți desena textul "Terminal" pe bara de titlu
        
        // 4. Desenăm caracterele TTY-ului în fereastra noastră!
        for (int row = 0; row < TERM_ROWS; row++) {
            for (int col = 0; col < TERM_COLS; col++) {
                uint16_t cell = tty_buffer[row * TERM_COLS + col];
                char c = cell & 0xFF;
                uint8_t fg = (cell >> 8) & 0x0F;
                uint8_t bg = (cell >> 12) & 0x0F;
                
                if (c == 0 || c == ' ') continue;

                // Aici folosești o funcție de desenat caracter (font) 
                // fix cum aveai în kernel, dar adaptată să scrie în window_fb
                // draw_char_to_fb(window_fb, col * 8, row * 16 + 20, c, vga_to_argb(fg));
            }
        }
        
        // 5. Trimitere framebuffer fereastră către ecran
        nano_gfx_present(win_x, win_y, WIN_WIDTH, WIN_HEIGHT, window_fb);
        
        // Oprim bucla dacă vrem să ieșim (ex. apasând ESC)
        // ... (Logica de mouse și butoane)
        
        nano_sleep(30); // 30 FPS
    }

    return 0;
}