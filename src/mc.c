#include "nano_libc.h"
#include "stdio.h"
#include "string.h"

#define MAX_FILES 64
#define MAX_NAME_LEN 64

// Codurile pentru taste (bazate pe driverul tău de tastatură din shell)
#define KEY_UP    128
#define KEY_DOWN  129
#define KEY_ENTER '\n'
#define KEY_CR    '\r'
#define KEY_ESC   27

static char file_list[MAX_FILES][MAX_NAME_LEN];
static int file_count = 0;
static int selected_index = 0;

// Încarcă fișierele din directorul curent în memorie
void load_current_directory() {
    file_count = 0;
    char filename[MAX_NAME_LEN];
    int index = 0;

    // Adăugăm opțiunea de întoarcere în directorul părinte
    strcpy(file_list[file_count++], "..");

    while (nano_get_file_name_at(index, filename, sizeof(filename)) && file_count < MAX_FILES) {
        // Evităm dublarea dacă ".." sau "." sunt returnate de sistem
        if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
            strcpy(file_list[file_count], filename);
            file_count++;
        }
        index++;
    }

    if (selected_index >= file_count) {
        selected_index = file_count - 1;
    }
    if (selected_index < 0) {
        selected_index = 0;
    }
}

// Desenează interfața pe ecran
void draw_interface() {
    nano_clear_screen();

    // Bara de titlu
    printf("==================================================\n");
    printf("                 NANO-MC v1.0                     \n");
    printf("==================================================\n");

    char cwd[128];
    if (nano_getcwd(cwd, sizeof(cwd)) <= 0) {
        strcpy(cwd, "/");
    }
    printf(" Calea curenta: %s\n\n", cwd);

    // Listarea fișierelor (afișăm un bloc de maximum 15 elemente)
    int start_row = 0;
    int max_visible = 15;
    if (selected_index >= max_visible) {
        start_row = selected_index - max_visible + 1;
    }

    for (int i = start_row; i < file_count && i < start_row + max_visible; i++) {
        if (i == selected_index) {
            // Elementul selectat (fundal evidențiat / text marcat)
            printf(" > [ %s ]\n", file_list[i]);
        } else {
            printf("   %s\n", file_list[i]);
        }
    }

    // Bara de meniu de jos
    printf("\n--------------------------------------------------\n");
    printf("[UP/DOWN] Navigare   [ENTER] Deschidere   [Q] Iesire\n");
}

int main() {
    load_current_directory();

    while (1) {
        draw_interface();

        unsigned char c = (unsigned char)nano_read_char();

        if (c == 'q' || c == 'Q') {
            nano_clear_screen();
            break;
        }
        else if (c == KEY_UP) {
            if (selected_index > 0) {
                selected_index--;
            }
        }
        else if (c == KEY_DOWN) {
            if (selected_index < file_count - 1) {
                selected_index++;
            }
        }
        else if (c == KEY_ENTER || c == KEY_CR) {
            // Dacă am apăsat Enter pe un element
            if (file_count > 0) {
                const char* target = file_list[selected_index];
                
                // Încercăm să schimbăm directorul
                if (nano_cd(target)) {
                    selected_index = 0;
                    load_current_directory();
                } else {
                    // Dacă nu e director, s-ar putea să fie un fișier pe care îl putem rula sau vizualiza
                    // Poți adăuga un mic afișaj sau rulare automată aici dacă dorești!
                }
            }
        }
    }

    return 0;
}