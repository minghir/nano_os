#include "io.h"
#include "idt.h"

uint16_t* VGA = (uint16_t*)0xB8000;
int cursor = 0;
volatile uint8_t keyboard_running = 1;

static void cursor_update() {
    uint16_t position = (uint16_t)cursor;

    outb(0x3D4, 0x0F);
    outb(0x3D5, position & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

void cursor_init() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x00);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
    cursor_update();
}

void print(const char* s) {
    while (*s) {
        char character = *s++;

        if (character == '\n') {
            cursor = (cursor / 80 + 1) * 80;
            continue;
        }

        if (character == '\b') {
            if (cursor > 0) {
                cursor--;
                VGA[cursor] = (0x0F << 8) | ' ';
            }
            continue;
        }

        VGA[cursor++] = (0x0F << 8) | character;
        if (cursor >= 80 * 25) {
            cursor = 0;
        }
    }
    cursor_update();
}

void newline() {
    cursor = (cursor / 80 + 1) * 80;
    if (cursor >= 80 * 25) {
        cursor = 0;
    }
    cursor_update();
}

void clear_screen() {
    for (int index = 0; index < 80 * 25; index++) {
        VGA[index] = (0x0F << 8) | ' ';
    }
    cursor = 0;
    cursor_update();
}

void pic_remap() {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void pic_enable_irq(int irq) {
    uint8_t mask = inb(0x21);
    mask &= ~(1 << irq);
    outb(0x21, mask);
}

static void print_uint8(uint8_t value) {
    char digits[4];
    int length = 0;

    if (value == 0) {
        print("0");
        return;
    }

    while (value > 0) {
        digits[length++] = '0' + (value % 10);
        value /= 10;
    }

    while (length > 0) {
        char character[2] = { digits[--length], 0 };
        print(character);
    }
}

static const char keyboard_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'',
    [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x',
    [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x39] = ' '
};

void keyboard_irq() {
    uint8_t scancode = inb(0x60);
    static uint8_t ctrl_pressed = 0;
    static uint8_t shift_pressed = 0;
    static uint8_t alt_pressed = 0;

    if (scancode == 0xE0) {
        return;
    }

    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x1D) {
            ctrl_pressed = 0;
        } else if (released == 0x2A || released == 0x36) {
            shift_pressed = 0;
        } else if (released == 0x38) {
            alt_pressed = 0;
        }
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0x38) {
        alt_pressed = 1;
        return;
    }
    if (ctrl_pressed && scancode == 0x2E) {
        keyboard_running = 0;
        return;
    }
    if (alt_pressed && scancode == 0x2E) {
        clear_screen();
        return;
    }

    if (scancode >= sizeof(keyboard_map)) {
        return;
    }

    char character = keyboard_map[scancode];
    if (character == 0) {
        return;
    }
    if (shift_pressed && character >= 'a' && character <= 'z') {
        character -= 'a' - 'A';
    }

    print("ASCII: ");
    print_uint8((uint8_t)character);
    print(" (");
    char output[2] = { character, 0 };
    print(output);
    print(")");
    newline();
}

void keyboard_stop_message() {
    print("Ctrl+C received. Keyboard loop stopped.");
    newline();
}

