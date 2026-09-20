#include "io.h"
#include "idt.h"
#include "timer.h"
#include "config.h"

volatile uint32_t timer_ticks = 0;

// Funcție helper pentru formatare cu zero în față (2-digits)
static void int_to_str_padded(uint8_t n, char* buf) {
    buf[0] = '0' + (n / 10);
    buf[1] = '0' + (n % 10);
    buf[2] = '\0';
}


void sleep_ms(uint32_t milliseconds) {
    // Calculăm câte tick-uri trebuie să așteptăm (presupunând 100 Hz: 1 tick = 10ms)
    // milliseconds / 10 sau (milliseconds * frequency) / 1000
    uint32_t ticks_to_wait = milliseconds / 10;
    uint32_t start_ticks = timer_ticks;

    // Așteptăm activ (busy-wait) până trece timpul
    while ((timer_ticks - start_ticks) < ticks_to_wait) {
        // Lăsăm procesorul în repaus până la următoarea întrerupere
        __asm__ volatile ("hlt");
    }
}

void timer_irq() {
    timer_ticks++;

    // Actualizăm o dată pe secundă (la fiecare 100 de tick-uri)
    if (timer_ticks % 100 == 0) {
        DateTime dt = get_current_time();
        
        char day_str[3], month_str[3], year_str[3];
        char hour_str[3], minute_str[3], second_str[3];

        int_to_str_padded(dt.day, day_str);
        int_to_str_padded(dt.month, month_str);
        int_to_str_padded(dt.year, year_str);
        int_to_str_padded(dt.hour, hour_str);
        int_to_str_padded(dt.minute, minute_str);
        int_to_str_padded(dt.second, second_str); // Am adăugat și secundele

        uint16_t* vga = (uint16_t*)0xB8000;
        int index = 0 * 80 + 62; // Am mutat puțin mai la stânga (coloana 62) pentru a face loc

        char* p = day_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
        vga[index++] = (0x0F << 8) | '/';
        
        p = month_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
        vga[index++] = (0x0F << 8) | '/';
        
        p = year_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
        
        vga[index++] = (0x0F << 8) | ' '; // Spațiu
        
        p = hour_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
        vga[index++] = (0x0F << 8) | ':';
        
        p = minute_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
        vga[index++] = (0x0F << 8) | ':'; // Separator pentru secunde
        
        p = second_str;
        vga[index++] = (0x0F << 8) | p[0];
        vga[index++] = (0x0F << 8) | p[1];
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

#include "io.h"

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