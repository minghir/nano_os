#include "io.h"
#include "config.h"
#include "shell.h"
#include "timer.h"

#define SHELL_INPUT_SIZE 128

// Funcție simplă de comparare a șirurilor de caractere
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void shell_init() {
    print("Nano OS Shell v0.1");
    newline();
    print("Type 'help' for available commands.");
    newline();
}

static void print_number(uint32_t value) {
    char buffer[11];
    int index = 10;
    buffer[index] = 0;

    do {
        buffer[--index] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    print(&buffer[index]);
}

static void print_time() {
    DateTime time = get_current_time();

    print("20");
    if (time.year < 10) print("0");
    print_number(time.year);
    print("-");
    if (time.month < 10) print("0");
    print_number(time.month);
    print("-");
    if (time.day < 10) print("0");
    print_number(time.day);
    print(" ");
    if (time.hour < 10) print("0");
    print_number(time.hour);
    print(":");
    if (time.minute < 10) print("0");
    print_number(time.minute);
    print(":");
    if (time.second < 10) print("0");
    print_number(time.second);
    newline();
}

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static void execute_command(char* command) {
    if (strcmp(command, "") == 0) {
        return;
    }
    if (strcmp(command, "help") == 0) {
        print("Commands: help, clear, time, echo, about");
        newline();
    } else if (strcmp(command, "clear") == 0) {
        clear_screen();
    } else if (strcmp(command, "time") == 0) {
        print_time();
    } else if (starts_with(command, "echo ")) {
        print(command + 5);
        newline();
    } else if (strcmp(command, "about") == 0) {
        print("Nano OS: educational x86-64 kernel");
        newline();
    } else {
        print("Unknown command: ");
        print(command);
        newline();
    }
}

void shell_run() {
    char input_buffer[SHELL_INPUT_SIZE];
    int length = 0;

    while (1) {
        print("nano> ");

        length = 0;
        input_buffer[0] = 0;
        while (1) {
            int value;
            while ((value = keyboard_read_char()) < 0) {
                __asm__ volatile ("hlt");
            }

            if (value == 3) {
                print("^C");
                newline();
                break;
            }
            if (value == '\b') {
                if (length > 0) {
                    length--;
                    input_buffer[length] = 0;
                    print("\b");
                }
                continue;
            }
            if (value == '\n') {
                newline();
                execute_command(input_buffer);
                break;
            }
            if (value < 32 || length >= SHELL_INPUT_SIZE - 1) {
                continue;
            }

            input_buffer[length++] = (char)value;
            input_buffer[length] = 0;
            char output[2] = { (char)value, 0 };
            print(output);
        }
    }
}