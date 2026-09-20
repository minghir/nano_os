#include "../io.h"

void kernel_main() {
    cursor_init();
    print("Nano OS booted!");
    newline();

    interrupts_init();

    print("Press keys. Ctrl+C stops the loop.");
    newline();

    while (keyboard_running) {
        __asm__ volatile ("hlt");
    }

    keyboard_stop_message();
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
