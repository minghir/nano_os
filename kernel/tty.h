#ifndef TTY_H
#define TTY_H

#include <stdint.h>

#define MAX_TTYS 4
#define MAX_TERM_COLS 128
#define MAX_TERM_ROWS 48

typedef struct {
    uint16_t screen_buffer[MAX_TERM_COLS * MAX_TERM_ROWS];
    int cursor;                     
    uint16_t current_vga_attr;      
    uint8_t keyboard_queue[256];    
    uint8_t keyboard_queue_read;
    uint8_t keyboard_queue_write;
	uint32_t foreground_pid; // PID-ul procesului care rulează acum în "față" pe acest TTY
} TTY;

// Le declarăm extern ca să poată fi folosite de io.c
extern TTY ttys[MAX_TTYS];
extern int active_tty;


void tty_init();
void tty_refresh_active_screen(void);
void switch_tty(int tty_index);
int get_active_tty();
void tty_clear_screen();


#endif