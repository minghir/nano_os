#include "libgfx.h"
#include "gfx_font1.h"


GfxCanvas* gfx_create_canvas(int width, int height) {
    GfxCanvas* canvas = (GfxCanvas*)nano_malloc(sizeof(GfxCanvas));
    if (!canvas) return 0;
    
    canvas->width = width;
    canvas->height = height;
    canvas->buffer = (uint32_t*)nano_malloc(width * height * sizeof(uint32_t));
    
    if (!canvas->buffer) {
        nano_free(canvas);
        return 0;
    }
    return canvas;
}

void gfx_free_canvas(GfxCanvas* canvas) {
    if (canvas) {
        if (canvas->buffer) nano_free(canvas->buffer);
        nano_free(canvas);
    }
}

void gfx_clear(GfxCanvas* canvas, uint32_t color) {
    if (!canvas || !canvas->buffer) return;
    int total = canvas->width * canvas->height;
    for (int i = 0; i < total; i++) {
        canvas->buffer[i] = color;
    }
}

void gfx_put_pixel(GfxCanvas* canvas, int x, int y, uint32_t color) {
    if (!canvas || !canvas->buffer) return;
    if (x >= 0 && x < canvas->width && y >= 0 && y < canvas->height) {
        canvas->buffer[y * canvas->width + x] = color;
    }
}

void gfx_draw_rect(GfxCanvas* canvas, int x, int y, int w, int h, uint32_t color) {
    for (int ry = 0; ry < h; ry++) {
        for (int rx = 0; rx < w; rx++) {
            gfx_put_pixel(canvas, x + rx, y + ry, color);
        }
    }
}

void gfx_draw_rect_outline(GfxCanvas* canvas, int x, int y, int w, int h, uint32_t color) {
    // Liniile orizontale (sus și jos)
    for (int rx = 0; rx < w; rx++) {
        gfx_put_pixel(canvas, x + rx, y, color);
        gfx_put_pixel(canvas, x + rx, y + h - 1, color);
    }
    // Liniile verticale (stânga și dreapta)
    for (int ry = 0; ry < h; ry++) {
        gfx_put_pixel(canvas, x, y + ry, color);
        gfx_put_pixel(canvas, x + w - 1, y + ry, color);
    }
}

void gfx_draw_char(GfxCanvas* canvas, int x, int y, char c_char, uint32_t fg_color, uint32_t bg_color) {
    unsigned char c = (unsigned char)c_char;
    const unsigned char* glyph = font8x16[c];

    for (int cy = 0; cy < 16; cy++) {
        int py = y + cy;
        // Dacă rândul este în afara canvasului, sărim Peste el instant
        if (py < 0 || py >= canvas->height) continue;

        uint8_t row_data = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            int px = x + cx;
            // Dacă pixelul e în afara lățimii, îl sărim
            if (px < 0 || px >= canvas->width) continue;

            if (row_data & (1 << (7 - cx))) {
                canvas->buffer[py * canvas->width + px] = fg_color;
            } else if (bg_color != 0x00000000) {
                canvas->buffer[py * canvas->width + px] = bg_color;
            }
        }
    }
}

void gfx_draw_string(GfxCanvas* canvas, int x, int y, const char* str, uint32_t fg_color, uint32_t bg_color) {
    int cursor_x = x;
    while (*str) {
        gfx_draw_char(canvas, cursor_x, y, *str, fg_color, bg_color);
        cursor_x += 8; // Deoarece lățimea unui caracter acum este de 8 pixeli fix
        str++;
    }
}

void gfx_present(GfxCanvas* canvas, int screen_x, int screen_y) {
    if (!canvas || !canvas->buffer) return;
    nano_gfx_present(screen_x, screen_y, canvas->width, canvas->height, canvas->buffer);
}

void gfx_draw_cursor(GfxCanvas* canvas, int x, int y) {
    // Matricea clasică de cursor tip săgeată (12x19 pixeli)
    // 'X' = Negru (Contur/Umbră), 'O' = Alb (Interior), ' ' = Transparent
    const char* cursor_shape[] = {
        "X           ",
        "XX          ",
        "X.X         ",
        "X..X        ",
        "X...X       ",
        "X....X      ",
        "X.....X     ",
        "X......X    ",
        "X.......X   ",
        "X........X  ",
        "X.....XXXX  ",
        "X..X..X     ",
        "X.X X..X    ",
        "XX  X..X    ",
        "X    X..X   ",
        "     X..X   ",
        "      XX    ",
        "      XX    ",
        "            "
    };

    int rows = 19;
    int cols = 12;

    for (int ry = 0; ry < rows; ry++) {
        for (int rx = 0; rx < cols; rx++) {
            char pixel_type = cursor_shape[ry][rx];
            if (pixel_type == ' ') continue; // Transparent

            uint32_t color = COLOR_WHITE; // Interiorul e alb
            if (pixel_type == 'X') {
                color = COLOR_BLACK;    // Conturul e negru
            }

            gfx_put_pixel(canvas, x + rx, y + ry, color);
        }
    }
}

void gfx_draw_window(GfxCanvas* canvas, int x, int y, int w, int h, const char* title) {
    uint32_t win_bg      = GFX_RGB(235, 235, 235); // Gri deschis (stil fereastră clasică)
    uint32_t title_bg    = GFX_RGB(10, 50, 100);   // Albastru închis pentru bară
    uint32_t title_fg    = COLOR_WHITE;            // Text alb
    uint32_t border_color= COLOR_BLACK;            // Contur negru exterior

    // 1. Corpul principal al ferestrei
    gfx_draw_rect(canvas, x, y, w, h, win_bg);

    // 2. Bara de titlu (în partea de sus, înălțime fixă de 24 pixeli)
    gfx_draw_rect(canvas, x, y, w, 24, title_bg);

    // 3. Titlul ferestrei (centrat vertical în bară, la y + 4)
    gfx_draw_string(canvas, x + 8, y + 4, title, title_fg, 0x00000000);

    // 4. Un buton simplu de închidere [X] în colțul din dreapta sus al barei
    int btn_size = 16;
    int btn_x = x + w - 20;
    int btn_y = y + 4;
    gfx_draw_rect(canvas, btn_x, btn_y, btn_size, btn_size, GFX_RGB(200, 50, 50)); // Roșu
    gfx_draw_char(canvas, btn_x + 4, btn_y, 'X', COLOR_WHITE, 0x00000000);

    // 5. Conturul exterior al întregii ferestre
    gfx_draw_rect_outline(canvas, x, y, w, h, border_color);
    
    // 6. O linie fină sub bara de titlu
    gfx_draw_rect_outline(canvas, x, y, w, 24, border_color);
}

void gfx_init_window(GfxWindow* win, int x, int y, int w, int h, const char* title) {
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->title = title;
    win->is_dragging = 0;
    win->closed = 0;
    // Alocăm un canvas intern pentru conținutul ferestrei (minus bara de titlu de 24px)
    win->canvas = gfx_create_canvas(w, h - 24); 
}

// Funcția de desenare pe baza obiectului fereastră
void gfx_draw_window_obj(GfxCanvas* main_canvas, GfxWindow* win) {
    if (win->closed) return;

    uint32_t win_bg      = GFX_RGB(235, 235, 235);
    uint32_t title_bg    = GFX_RGB(10, 50, 100);
    uint32_t title_fg    = COLOR_WHITE;
    uint32_t border_color= COLOR_BLACK;

    // 1. Desenăm corpul ferestrei pe canvasul principal
    gfx_draw_rect(main_canvas, win->x, win->y, win->w, win->h, win_bg);

    // 2. Copiere rapidă a conținutului intern (Fast Blit cu rânduri memcpy)
    if (win->canvas && win->canvas->buffer) {
        int src_x = 0, src_y = 0;
        int dest_x = win->x;
        int dest_y = win->y + 24; // Sub bara de titlu
        int width = win->canvas->width;
        int height = win->canvas->height;

        // Validări de margini (clipping) înainte de desenare
        if (dest_x < 0) { src_x -= dest_x; width += dest_x; dest_x = 0; }
        if (dest_y < 0) { src_y -= dest_y; height += dest_y; dest_y = 0; }
        if (dest_x + width > main_canvas->width) { width = main_canvas->width - dest_x; }
        if (dest_y + height > main_canvas->height) { height = main_canvas->height - dest_y; }

        // Dacă fereastra este în interiorul ecranului, copiem rând cu rând extrem de rapid
        if (width > 0 && height > 0) {
            for (int cy = 0; cy < height; cy++) {
                uint32_t* src_row = &win->canvas->buffer[(src_y + cy) * win->canvas->width + src_x];
                uint32_t* dest_row = &main_canvas->buffer[(dest_y + cy) * main_canvas->width + dest_x];
                
                // memcpy mută un rând întreg de pixeli dintr-o singură mișcare hardware-optimizată
                memcpy(dest_row, src_row, width * sizeof(uint32_t));
            }
        }
    }

    // 3. Desenăm bara de titlu deasupra conținutului
    gfx_draw_rect(main_canvas, win->x, win->y, win->w, 24, title_bg);
    gfx_draw_string(main_canvas, win->x + 8, win->y + 4, win->title, title_fg, 0x00000000);

    // 4. Butonul [X] de închidere
    int btn_size = 16;
    int btn_x = win->x + win->w - 20;
    int btn_y = win->y + 4;
    gfx_draw_rect(main_canvas, btn_x, btn_y, btn_size, btn_size, GFX_RGB(200, 50, 50));
    gfx_draw_char(main_canvas, btn_x + 4, btn_y, 'X', COLOR_WHITE, 0x00000000);

    // 5. Contururi
    gfx_draw_rect_outline(main_canvas, win->x, win->y, win->w, win->h, border_color);
    gfx_draw_rect_outline(main_canvas, win->x, win->y, win->w, 24, border_color);
}

// Logica de interacțiune cu mouse-ul (Returnează 0 dacă fereastra e activă, 1 dacă s-a închis)
static int last_mouse_btn = 0;
int gfx_update_window_mouse(GfxWindow* win, int mouse_x, int mouse_y, uint8_t mouse_buttons) {
    if (win->closed) return 1;

    int left_clicked = (mouse_buttons & 1) && !(last_mouse_btn & 1);
    int left_released = !(mouse_buttons & 1) && (last_mouse_btn & 1);

    // Coordonatele butonului [X]
    int btn_x = win->x + win->w - 20;
    int btn_y = win->y + 4;

    // Dacă utilizatorul eliberează click-ul, oprim tragerea (dragging)
    if (left_released) {
        win->is_dragging = 0;
    }

    // Dacă e apăsat click stânga
    if (left_clicked) {
        // Verificăm dacă s-a dat click pe butonul [X] (în colțul din dreapta sus)
        if (mouse_x >= btn_x && mouse_x < btn_x + 16 && mouse_y >= btn_y && mouse_y < btn_y + 16) {
            win->closed = 1;
            last_mouse_btn = mouse_buttons;
            return 1; // Fereastra a fost închisă
        }

        // Verificăm dacă s-a dat click pe bara de titlu (pentru a începu mutarea ferestrei)
        if (mouse_x >= win->x && mouse_x < win->x + win->w && mouse_y >= win->y && mouse_y < win->y + 24) {
            win->is_dragging = 1;
        }
    }

    // Dacă fereastra este în mod "dragging" și mouse-ul se mișcă, actualizăm poziția
    // (Aici am putea reține un offset de la colțul ferestrei, dar pentru simplitate o mutăm direct)
    if (win->is_dragging && (mouse_buttons & 1)) {
        // O simplă centrare relativă sau urmărire fină
        win->x = mouse_x - (win->w / 2);
        win->y = mouse_y - 12; // Ținem mouse-ul centrat vertical pe bara de titlu
    }

    last_mouse_btn = mouse_buttons;
    return 0;
}

// --- Window Manager Functions ---
GfxWindowManager* gfx_wm_create(void) {
    GfxWindowManager* wm = (GfxWindowManager*)nano_malloc(sizeof(GfxWindowManager));
    if (!wm) return 0;
    wm->windows = list_create();
    wm->active_window = 0;
    return wm;
}

void gfx_wm_free(GfxWindowManager* wm) {
    if (!wm) return;
    list_free(wm->windows);
    nano_free(wm);
}

void gfx_wm_add_window(GfxWindowManager* wm, GfxWindow* win) {
    if (!wm || !win) return;
    list_push_back(wm->windows, win);
    wm->active_window = win;
}

void gfx_wm_draw_all(GfxWindowManager* wm, GfxCanvas* canvas) {
    if (!wm || !wm->windows) return;
    Node* current = wm->windows->head;
    while (current) {
        GfxWindow* win = (GfxWindow*)current->data;
        if (!win->closed) {
            gfx_draw_window_obj(canvas, win);
        }
        current = current->next;
    }
}

void gfx_wm_handle_mouse(GfxWindowManager* wm, int mouse_x, int mouse_y, uint8_t mouse_buttons) {
    if (!wm || !wm->windows) return;

    static int prev_buttons = 0;
    int left_clicked = (mouse_buttons & 1) && !(prev_buttons & 1);
    int left_released = !(mouse_buttons & 1) && (prev_buttons & 1);

    if (left_released && wm->active_window) {
        wm->active_window->is_dragging = 0;
    }

    if (left_clicked) {
        // Căutăm de la față spre spate (tail -> head)
        Node* current = wm->windows->tail;
        GfxWindow* clicked_win = 0;
        Node* clicked_node = 0;

        while (current) {
            GfxWindow* win = (GfxWindow*)current->data;
            if (!win->closed) {
                if (mouse_x >= win->x && mouse_x < win->x + win->w &&
                    mouse_y >= win->y && mouse_y < win->y + win->h) {
                    clicked_win = win;
                    clicked_node = current;
                    break;
                }
            }
            current = current->prev;
        }

        if (clicked_win && clicked_node) {
            wm->active_window = clicked_win;
            list_to_tail(wm->windows, clicked_node); // Z-Order bring-to-front

            int btn_x = clicked_win->x + clicked_win->w - 20;
            int btn_y = clicked_win->y + 4;
            if (mouse_x >= btn_x && mouse_x < btn_x + 16 && mouse_y >= btn_y && mouse_y < btn_y + 16) {
                clicked_win->closed = 1;
            } else if (mouse_y >= clicked_win->y && mouse_y < clicked_win->y + 24) {
                clicked_win->is_dragging = 1;
            }
        }
    }

    if (wm->active_window && wm->active_window->is_dragging && (mouse_buttons & 1)) {
        wm->active_window->x = mouse_x - (wm->active_window->w / 2);
        wm->active_window->y = mouse_y - 12;
    }

    prev_buttons = mouse_buttons;
}