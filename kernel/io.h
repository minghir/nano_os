#ifndef IO_H
#define IO_H

#include <stdint.h>

#define FONT_COLOR (0x0A << 8)
// 0x0F - Alb (White)
// 0x0A - Verde deschis (Light Green)
// 0x06 - Maro (Brown)
// 0x0E - Galben deschis (Yellow)
// 0x0C - Roșu deschis (Light Red)

// VGA text mode
extern uint16_t* VGA;
extern int cursor;

extern uint16_t current_vga_attr;

void cursor_update();

// I/O port access
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait() {
    outb(0x80, 0);
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a" (value), "Nd" (port));
}

// --- ADĂUGATE PENTRU PCI (32-bit) ---
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "d"(port));
}
// ------------------------------------


extern volatile uint8_t keyboard_running;


void tty_write_char(int tty_id, char c, uint16_t attr);
void print(const char* s);
void print_syslog(const char* s);
void newline();
void clear_screen();
void print_at(int row, int col, const char* s);
void cursor_init();

// PIC + IRQ
void pic_remap();
void pic_enable_irq(int irq);
void keyboard_irq();
int keyboard_read_char();
void keyboard_read_line(char* buffer, uint32_t max_length);
void keyboard_stop_message();
void keyboard_flush();


// Interrupts
extern void isr_keyboard();
void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags);
void interrupts_init();

//void set_vga_color_palette();

int keyboard_has_data(void);

void set_cursor_shape(int style);

#endif

