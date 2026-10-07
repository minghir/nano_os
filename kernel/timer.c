#include "io.h"
#include "idt.h"
#include "timer.h"
#include "config.h"
#include "mouse.h"

volatile uint32_t timer_ticks = 0;

extern void cursor_blink_tick();

extern volatile int mouse_moved;
extern int active_tty;
extern void gfx_redraw_tty(int tty_id);

// Funcție helper pentru formatare cu zero în față (2-digits)
static void int_to_str_padded(uint8_t n, char* buf) {
    buf[0] = '0' + (n / 10);
    buf[1] = '0' + (n % 10);
    buf[2] = '\0';
}


void sleep_ms(uint32_t milliseconds) {
    uint32_t start_ticks = timer_ticks;

    // Așteptăm activ (busy-wait) până trece timpul cerut
    while ((timer_ticks - start_ticks) < milliseconds) {
        __asm__ volatile ("hlt"); // CPU-ul stă pe bară până la următorul IRQ0
    }
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 100000; j++) {
            __asm__ volatile("nop");
        }
    }
}


void timer_irq() {
    timer_ticks++;
	
	cursor_blink_tick();
	
	// 2. DACĂ MOUSE-UL S-A MIȘCAT, REDESENĂM ECRANUL IMEDIAT!
    if (mouse_moved) {
        mouse_moved = 0;
        gfx_redraw_tty(active_tty);
    }

    
}

void timer_init(uint32_t frequency) {
    // Calculează divizorul
    uint32_t divisor = 1193180 / frequency;

    // Setează comanda pentru PIT (Canalul 0, Access lobyte/hibyte, Mod 3 - Square Wave Generator)
    outb(0x43, 0x36);

    // Trimite partea low și partea high a divizorului
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}



// Funcție helper pentru conversia din BCD în zecimal
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

// Citește un registru CMOS
uint8_t read_rtc(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}




DateTime get_current_time() {
    DateTime dt;
    dt.second = bcd_to_bin(read_rtc(0x00));
    dt.minute = bcd_to_bin(read_rtc(0x02));
    dt.hour   = bcd_to_bin(read_rtc(0x04));
    dt.day    = bcd_to_bin(read_rtc(0x07));
    dt.month  = bcd_to_bin(read_rtc(0x08));
    dt.year   = bcd_to_bin(read_rtc(0x09));

    // Folosim offset-ul din fișierul de configurare în loc de '3' hardcodat
    dt.hour += current_config.timezone_offset;
    
    if (dt.hour >= 24) {
        dt.hour -= 24;
        dt.day += 1;
    } else if (dt.hour < 0) {
        dt.hour += 24;
        dt.day -= 1;
    }

    return dt;
}

