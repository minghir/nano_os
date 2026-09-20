#include "io.h"
#include "idt.h"

extern void isr_keyboard();

void interrupts_init() {
    __asm__ volatile ("cli");

    pic_remap();
    idt_set_gate(9, (uint64_t)isr_keyboard, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)isr_keyboard, 0x08, 0x8E);
    idt_load();

    pic_enable_irq(1);
    __asm__ volatile ("sti");
}

