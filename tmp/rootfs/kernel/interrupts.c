#include "io.h"
#include "idt.h"
#include "timer.h"

// Declarăm funcțiile ISR din fișierele de assembly
extern void isr_keyboard();
extern void isr_timer();
extern void isr_syscall();
extern void isr14();
extern void isr_mouse(); 
extern void isr_e1000(); // ISR-ul pentru e1000 / IRQ 11


void interrupts_init() {
    // 1. Oprește întreruperile global în timpul configurării
    __asm__ volatile ("cli");

    // 2. Remapează PIC-ul (Master: 32-39, Slave: 40-47)
    pic_remap();

    // 3. Setează porțile în IDT
    idt_set_gate(32, (uint64_t)isr_timer, 0x08, 0x8E);     // IRQ 0 - Timer
    idt_set_gate(33, (uint64_t)isr_keyboard, 0x08, 0x8E);  // IRQ 1 - Tastatură
    idt_set_gate(43, (uint64_t)isr_e1000, 0x08, 0x8E);     // IRQ 11 - Intel e1000 (<--- ADAUGĂ ASTA)
    idt_set_gate(44, (uint64_t)isr_mouse, 0x08, 0x8E);     // IRQ 12 - Mouse PS/2
    
    idt_set_gate(0x80, (uint64_t)isr_syscall, 0x08, 0xEE); // Syscalls
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);         // Page Fault
	
	// Adaugă astea în interrupts_init(), lângă idt_set_gate(14...
    idt_set_gate(8, (uint64_t)isr14, 0x08, 0x8E);  // Prinde Double Fault
    idt_set_gate(13, (uint64_t)isr14, 0x08, 0x8E); // Prinde GPF

    // Încarcă tabelul IDT în procesor folosind LIDT
    idt_load();

    // 4. Deblochează IRQ 0 (Timer), IRQ 1 (Tastatură) și IRQ 2 (Cascade) pe Master PIC
    uint8_t mask = inb(0x21);
    mask &= ~((1 << 0) | (1 << 1) | (1 << 2));  
    outb(0x21, mask);

    // 5. Deblochează IRQ 11 și IRQ 12 pe Slave PIC (port 0xA1)
    uint8_t slave_mask = inb(0xA1);
    slave_mask &= ~((1 << 3) | (1 << 4)); // Bit 3 pentru IRQ 11 (11-8=3), Bit 4 pentru IRQ 12 (12-8=4)
    outb(0xA1, slave_mask);
}