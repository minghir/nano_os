#include "shell.h"
#include "io.h"
#include "config.h"
#include "shell.h"
#include "timer.h"
#include "memory.h"
#include "ata.h"
#include "fs.h"
#include "string.h"


void execute_command(char* command) {
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
    else if (starts_with(command, "write")) {
        // Exemplu simplu: "write <nume> <text>"
        // Pentru simplitate, luăm textul de după numele fișierului
        char* args = command + 6;
        while (*args == ' ') args++; // Sărim peste spații

        // Găsim primul spațiu care desparte numele de text
        char* text = args;
        while (*text != ' ' && *text != '\0') text++;

        if (*text == '\0') {
            print("Utilizare: write <fisier> <text>");
            newline();
        } else {
            *text = '\0'; // Separăm numele
            text++;       // Trecem la text

            char* filename = args;
            if (fs_write_file(filename, (uint8_t*)text, string_length(text))) {
                print("Scriere reusita in fisier!");
                newline();
            } else {
                print("Eroare: Fisierul nu a fost gasit sau nu s-a putut scrie.");
                newline();
            }
        }
    } else if (starts_with(command, "touch")) {
        char* filename = command + 6; 
        while (*filename == ' ') filename++; // Sărim peste spații

        if (*filename == '\0') {
            print("Utilizare: touch <nume>");
            newline();
        } else {
            // Nu mai trimitem sectorul hardcodat; cerem crearea cu mărime inițială 0
            if (fs_create_file(filename, 0)) {
                print("Fisier creat cu succes!");
                newline();
            } else {
                print("Eroare la crearea fisierului (director plin)!");
                newline();
            }
        }
    }else if (starts_with(command, "rm")) {
        char* filename = command + 3; // Sărim peste "rm "
        while (*filename == ' ') filename++;

        if (*filename == '\0') {
            print("Utilizare: rm <fisier>");
            newline();
        } else {
            if (fs_delete_file(filename)) {
                print("Fisier sters cu succes!");
                newline();
            } else {
                print("Eroare: Fisierul nu a fost gasit.");
                newline();
            }
        }
    }
    else if (starts_with(command, "cat")) {
        char* filename = command + 4; // Sărim peste "cat "
        while (*filename == ' ') filename++;

        if (*filename == '\0') {
            print("Utilizare: cat <fisier>");
            newline();
        } else {
            uint8_t* file_buffer = (uint8_t*)malloc(512);
            if (!file_buffer) {
                print("Eroare de memorie!");
                newline();
            } else {
                for (int i = 0; i < 512; i++) file_buffer[i] = 0;

                int bytes_read = fs_read_file(filename, file_buffer, 511);
                if (bytes_read > 0) {
                    print((char*)file_buffer);
                    newline();
                } else {
                    print("Fisierul nu a fost gasit sau este gol.");
                    newline();
                }
            }
        }
    } 
    else if (starts_with(command, "mkdir ")) {
        char* dirname = command + 6;
        while (*dirname == ' ') dirname++;

        if (fs_mkdir(dirname)) {
            print("Director creat cu succes!");
            newline();
        } else {
            print("Eroare: Nume deja existent sau director plin.");
            newline();
        }
    } else if (starts_with(command, "cd ")) {
        char* path = command + 3;
        while (*path == ' ') path++;

        // Dacă sistemul de fișiere ne validează că există calea
        if (fs_cd(path)) {
            // Abia acum modificăm vizual textul din consolă
            update_prompt_path(path);
            // (Opțional: poți scoate print("Calea a fost schimbata") 
            // pentru a se comporta ca Linux, unde cd e silențios)
        } else {
            print("Eroare: Calea nu exista.");
            newline();
        }
    }
     else if (starts_with(command, "rmdir ")) {
        char* dirname = command + 6;
        while (*dirname == ' ') dirname++;

        if (fs_rmdir(dirname)) {
            print("Director sters.");
            newline();
        } else {
            print("Eroare la stergere (probabil nu e director sau nu exista).");
            newline();
        }
    } 
    else if (starts_with(command, "exec ")) {
        char* filename = command + 5;
        while (*filename == ' ') filename++;

        // 1. Alocăm memoria la adresa fixă (8 MB)
        uint8_t* prog_memory = (uint8_t*)0x800000;

        // 2. Citim programul de pe disc
        int bytes_read = fs_read_file(filename, prog_memory, 16384);
        
        if (bytes_read > 0) {
            // --- AICI PUNEM DEBUG-UL ---
            print("Fisier citit, dimensiune: ");
            print_number(bytes_read); 
            print(" bytes");
            newline();
            // ---------------------------

            print("Executam programul..."); 
            newline();
            
            // 3. Transformăm adresa în pointer de funcție și o executăm
            void (*program_start)(void) = (void (*)(void))prog_memory;
            program_start();

        } else {
            print("Eroare: Fisierul nu exista sau nu a putut fi citit."); newline();
        }
    }
    else if (strcmp(command, "make_app") == 0) {
        // Codul mașină pur (x86-64) pentru: mov eax, 42; ret;
        uint8_t machine_code[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
        
        // Creăm fișierul și scriem programul în el
        if (fs_create_file("app.bin", sizeof(machine_code))) {
            fs_write_file("app.bin", machine_code, sizeof(machine_code));
            print("Programul 'app.bin' a fost compilat pe disc!"); newline();
        }
    }
    else if (strcmp(command, "fdisk") == 0) {
        fs_fdisk();
    }else if (strcmp(command, "format") == 0) {
        // Avertizăm utilizatorul
        print("ATENTIE: Aceasta actiune va sterge IREREVERSIBIL toate fisierele!"); newline();
        print("Pentru a continua, tasteaza comanda: format y"); newline();
        
    } else if (strcmp(command, "format y") == 0 || strcmp(command, "format yes") == 0) {
        // Executăm formatarea doar dacă a confirmat clar
        fs_format();
    }
    else {
        print("Unknown command: ");
        print(command);
        newline();
    }
}