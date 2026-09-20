#ifndef IO_H
#define IO_H

#include <stdint.h>

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

// VGA text mode
extern uint16_t* VGA;
extern int cursor;

void print(const char* s);
void newline();
void clear_screen();
void print_at(int row, int col, const char* s);
void cursor_init();

// PIC + IRQ
void pic_remap();
void pic_enable_irq(int irq);
void keyboard_irq();
int keyboard_read_char();
void keyboard_stop_message();
extern volatile uint8_t keyboard_running;

// Interrupts
extern void isr_keyboard();
void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags);
void interrupts_init();


#endif

