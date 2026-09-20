#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

void idt_load() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

