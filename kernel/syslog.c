#include "syslog.h"
#include "io.h"
#include "fs.h"
#include "string.h"

#include <stdint.h>

char kernel_log_buffer[KERNEL_LOG_SIZE];

uint32_t log_head = 0;

void kernel_log(const char* message) {
    // Opțional: putem afișa direct pe ecran sau păstra doar în buffer pentru dmesg
    // print(message); 
    
    int i = 0;
    while (message[i] != '\0') {
        kernel_log_buffer[log_head] = message[i];
        log_head = (log_head + 1) % KERNEL_LOG_SIZE;
        i++;
    }
    kernel_log_buffer[log_head] = '\0';
}

// Funcția care se apelează la CRASH / KERNEL PANIC
void kernel_panic(const char* reason) {
    print("\n================ KERNEL PANIC ================\n");
    print(reason);
    print("\nSe salveaza logurile pe disc in /kernel.log...\n");

    // Salvăm logurile direct pe disc folosind funcția internă a sistemului de fișiere
    // (Asigură-te că funcția de scriere a fișierelor din kernel poate fi apelată aici)
	//fs_create_file("/kernel.log", KERNEL_LOG_SIZE);
    fs_write_file("/kernel.log", (uint8_t*)kernel_log_buffer, string_length(kernel_log_buffer));

    print("Logurile au fost salvate. Sistemul este oprit.\n");
    print("==============================================\n");

    // Oprim complet CPU-ul
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}