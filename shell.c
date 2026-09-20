#include "io.h"
#include "config.h"
#include "shell.h"
#include "timer.h"
#include "memory.h"
#include "ata.h"
#include "fs.h"

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
        // <-- 2. Am adaugat 'alloc' în lista de comenzi
        print("Commands: help, clear, time, echo, about, alloc");
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
    } else if (strcmp(command, "alloc") == 0) {
        // <-- 3. Logica de test pentru malloc folosind Bump Allocator
        void* ptr = malloc(64); // Încercăm să alocăm 64 de octeți
        if (ptr == 0) {
            print("Out of memory!");
            newline();
        } else {
            print("Malloc success! Allocated 64 bytes.");
            newline();
        }
    } else if (strcmp(command, "alloc_str") == 0) {
        // Alocăm 128 de octeți în heap
        char* dynamic_str = (char*)malloc(128);
        
        if (dynamic_str == 0) {
            print("Out of memory!");
            newline();
            return;
        } else {
            // Scriem manual câteva caractere în memoria alocată
            dynamic_str[0] = 'H';
            dynamic_str[1] = 'e';
            dynamic_str[2] = 'a';
            dynamic_str[3] = 'p';
            dynamic_str[4] = ' ';
            dynamic_str[5] = 'w';
            dynamic_str[6] = 'o';
            dynamic_str[7] = 'r';
            dynamic_str[8] = 'k';
            dynamic_str[9] = 's';
            dynamic_str[10] = '!';
            dynamic_str[11] = '\0'; // Terminatorul de șir este obligatoriu!

            print("Scris din heap: ");
            print(dynamic_str);
            newline();
        }
    }else if (strcmp(command, "stats") == 0) {
        print("--- Memory Statistics ---");
        newline();
        print("Total Heap: ");
        print_number(get_heap_total());
        print(" bytes");
        newline();
        print("Used Heap:  ");
        print_number(get_heap_used());
        print(" bytes");
        newline();
        print("Free Heap:  ");
        print_number(get_heap_free());
        print(" bytes");
        newline();
    }else if (starts_with(command, "store ")) {
        // Preluăm textul de după "store "
        char* source_text = command + 6; 
        
        // Calculăm lungimea șirului de caractere
        int length = 0;
        while (source_text[length] != '\0') {
            length++;
        }

        // Alocăm memorie exact pentru lungimea textului + 1 octet pentru terminatorul '\0'
        char* heap_buffer = (char*)malloc(length + 1);

        if (heap_buffer == 0) {
            print("Out of memory!");
            newline();
        } else {
            // Copiem caracterele unul câte unul în memoria alocată din heap
            int i = 0;
            while (source_text[i] != '\0') {
                heap_buffer[i] = source_text[i];
                i++;
            }
            heap_buffer[i] = '\0'; // Închidem șirul corect

            // Afișăm confirmarea citind direct din heap
            print("Citit din Heap: ");
            print(heap_buffer);
            newline();
        }
    } else if (starts_with(command, "sleep ")) {
        // Preluăm argumentul de după "sleep "
        const char* arg = command + 6;
        int seconds = simple_atoi(arg);

        if (seconds <= 0) {
            print("Invalid time or usage: sleep <seconds>");
            newline();
        } 
        else {
            print("Waitting for ");
            print_number(seconds);
            print(" secunde...");
            newline();

            // Transformăm secundele în milisecunde pentru funcția ta sleep_ms
            sleep_ms(seconds * 1000);

            print("Gata!");
            newline();
        }
    }else if (starts_with(command, "readdisk")) {
            // Alocăm un buffer de 512 octeți pentru sectorul de disc
            uint8_t* sector_buffer = (uint8_t*)malloc(512);

            if (sector_buffer == 0) {
                print("Out of memory for disk buffer!");
                newline();
            } else {
                // Citim sectorul 0 (primul sector / MBR)
                disk_read_sector(0, sector_buffer);

                print("Sector 0 citit cu succes! Primele caractere:");
                newline();

                // Afișăm primii 64 de octeți ca text (sau poți afișa tot ce vrei)
                for (int i = 0; i < 64; i++) {
                    char c = (char)sector_buffer[i];
                    // Afișăm doar caractere vizibile, altfel punem un punct
                    if (c >= 32 && c <= 126) {
                        char buf[2] = {c, '\0'};
                        print(buf);
                    } else {
                        print(".");
                    }
                }
                newline();
            }
        }
    else if (starts_with(command, "writedisk")) {
        uint8_t* sector_buffer = (uint8_t*)malloc(512);

        if (sector_buffer == 0) {
            print("Out of memory for disk buffer!");
            newline();
        } else {
            // Curățăm bufferul și punem un mesaj text la început
            for (int i = 0; i < 512; i++) {
                sector_buffer[i] = 0;
            }
            
            // Un mesaj scurt pe care îl scriem pe disc
            char* msg = "Salut din Nano OS! Date persistente pe HDD.";
            int i = 0;
            while (msg[i] != '\0' && i < 511) {
                sector_buffer[i] = (uint8_t)msg[i];
                i++;
            }

            // Scriem în sectorul 0 (sau poți alege alt sector, ex: 1)
            disk_write_sector(0, sector_buffer);

            print("Scriere reusita in sectorul 0!");
            newline();
        }
    }
    else if (starts_with(command, "ls")) {
        fs_list_files();
    }  
    else if (starts_with(command, "touch")) {
        // Luăm numele fișierului de după comandă (ex: "touch test")
        char* filename = command + 6; 
        if (*filename == '\0') {
            print("Utilizare: touch <nume>");
            newline();
        } else {
            // Pentru început, punem fișierul să înceapă la sectorul 2, mărime 512 octeți
            if (fs_create_file(filename, 2, 512)) {
                print("Fisier creat cu succes!");
                newline();
            } else {
                print("Eroare la crearea fisierului (director plin sau neinit)!");
                newline();
            }
        }
    }  
    else {
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