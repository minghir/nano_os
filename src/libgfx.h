#ifndef LIBGFX_H
#define LIBGFX_H

#include <stdint.h>
#include "nano_libc.h"
#include "libds.h" // Avem nevoie de listele înlănțuite pentru ferestre multiple

// Structură simplă pentru culori ARGB (32-biți)
#define GFX_RGB(r, g, b) (0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF))

// Culori predefinite utile
#define COLOR_BLACK   0xFF000000
#define COLOR_WHITE   0xFFFFFFFF
#define COLOR_RED     0xFFFF0000
#define COLOR_GREEN   0xFF00FF00
#define COLOR_BLUE    0xFF0000FF
#define COLOR_GRAY    0xFF808080
#define COLOR_DARK_BG 0xFF1A2B3C

// Context grafic (Canvas local pentru o aplicație sau o fereastră)
typedef struct {
    uint32_t* buffer;
    int width;
    int height;
} GfxCanvas;

// Funcții de bază pentru Canvas
GfxCanvas* gfx_create_canvas(int width, int height);
void gfx_free_canvas(GfxCanvas* canvas);
void gfx_clear(GfxCanvas* canvas, uint32_t color);

// Primitive de desenare
void gfx_put_pixel(GfxCanvas* canvas, int x, int y, uint32_t color);
void gfx_draw_rect(GfxCanvas* canvas, int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect_outline(GfxCanvas* canvas, int x, int y, int w, int h, uint32_t color);
void gfx_draw_char(GfxCanvas* canvas, int x, int y, char c, uint32_t fg_color, uint32_t bg_color);
void gfx_draw_string(GfxCanvas* canvas, int x, int y, const char* str, uint32_t fg_color, uint32_t bg_color);
void gfx_draw_cursor(GfxCanvas* canvas, int x, int y);

// Randare pe ecran
void gfx_present(GfxCanvas* canvas, int screen_x, int screen_y);

// --- FERESTRE ȘI INTERACTIVITATE ---
typedef struct {
    int x, y;          
    int w, h;          
    const char* title; 
    int is_dragging;   
    int closed;        
    GfxCanvas* canvas; // <--- Canvasul intern al ferestrei (buffer propriu)
} GfxWindow;

void gfx_init_window(GfxWindow* win, int x, int y, int w, int h, const char* title);
void gfx_draw_window(GfxCanvas* canvas, int x, int y, int w, int h, const char* title);
void gfx_draw_window_obj(GfxCanvas* canvas, GfxWindow* win);
int gfx_update_window_mouse(GfxWindow* win, int mouse_x, int mouse_y, uint8_t mouse_buttons);

// --- WINDOW MANAGER INTEGRAT ÎN LIBGFX ---
typedef struct {
    LinkedList* windows;       // Lista de ferestre (folosește libds)
    GfxWindow* active_window;  // Fereastra în focus (deasupra tuturor)
} GfxWindowManager;

GfxWindowManager* gfx_wm_create(void);
void gfx_wm_free(GfxWindowManager* wm);
void gfx_wm_add_window(GfxWindowManager* wm, GfxWindow* win);
void gfx_wm_draw_all(GfxWindowManager* wm, GfxCanvas* canvas);
void gfx_wm_handle_mouse(GfxWindowManager* wm, int mouse_x, int mouse_y, uint8_t mouse_buttons);

#endif