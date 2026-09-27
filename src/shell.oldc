#include "io.h"
#include "config.h"
#include "shell.h"
#include "timer.h"
#include "memory.h"
#include "ata.h"
#include "fs.h"
#include "string.h"

#define SHELL_INPUT_SIZE 256
static char current_path[256] = "/";

int string_length(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

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

void print_number(uint32_t value) {
    char buffer[11];
    int index = 10;
    buffer[index] = 0;

    do {
        buffer[--index] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    print(&buffer[index]);
}

void print_time() {
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

int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

void update_prompt_path(const char* cd_arg) {
    if (strcmp(cd_arg, "/") == 0) {
        current_path[0] = '/';
        current_path[1] = '\0';
        return;
    }

    // Dacă e cale absolută, resetăm promptul virtual la root
    if (cd_arg[0] == '/') {
        current_path[0] = '/';
        current_path[1] = '\0';
        cd_arg++;
    }

    while (*cd_arg) {
        while (*cd_arg == '/') cd_arg++;
        if (*cd_arg == '\0') break;

        char folder[12];
        int i = 0;
        while (*cd_arg && *cd_arg != '/' && i < 11) {
            folder[i++] = *cd_arg++;
        }
        folder[i] = '\0';
        while (*cd_arg && *cd_arg != '/') cd_arg++;

        if (strcmp(folder, "..") == 0) {
            // Ștergem ultimul folder din string-ul curent
            int len = string_length(current_path);
            if (len > 1 && current_path[len - 1] == '/') len--;
            while (len > 0 && current_path[len - 1] != '/') len--;
            
            if (len == 0) { current_path[0] = '/'; current_path[1] = '\0'; }
            else if (len == 1) current_path[1] = '\0'; // Lăsăm doar root-ul
            else current_path[len - 1] = '\0';
        } else if (strcmp(folder, ".") != 0) {
            // Adăugăm noul folder la string
            int len = string_length(current_path);
            if (len > 0 && current_path[len - 1] != '/') current_path[len++] = '/';
            int j = 0;
            while (folder[j] && len < 254) current_path[len++] = folder[j++];
            current_path[len] = '\0';
        }
    }
}



void shell_run() {
    char input_buffer[SHELL_INPUT_SIZE];
    int length = 0;

    while (1) {
        print(current_path);
        print("# ");

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
                execute_command(trim(input_buffer));
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