#ifndef GFX_TERM_H
#define GFX_TERM_H

#include <stdint.h>

void gterm_init(uint64_t fb_addr, uint32_t width, uint32_t height, uint32_t pitch);
//void draw_char(int col, int row, unsigned char c, uint32_t fg, uint32_t bg);
void gterm_putchar(char c);
void gterm_print(const char* str);
void gterm_clear();
void gterm_draw_char(int col, int row, unsigned char c, uint32_t fg, uint32_t bg);
void gterm_draw_cursor_box(int x, int y, uint32_t color);
#endif