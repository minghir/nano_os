#include "../src/io.h"
#include "../src/config.h"
//#include "../src/shell.h"
#include "../src/memory.h"
#include "../src/fs.h"
#include "../src/string.h"
#include <stdint.h>
/*
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} __attribute__((packed)) multiboot_info_t;
*/

// Definiție parțială a structurii Multiboot Info
typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    // ... alte câmpuri, iar spre sfârșit (dacă flags & (1 << 12)) se află datele VBE / Framebuffer:
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    
    // Datele directe despre Framebuffer (Multiboot Specification)
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    // ... culori specifice
} __attribute__((packed)) multiboot_info_t;



typedef struct mod_list {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
} mod_list_t;

char env_path[128] = "/;/bin";

void load_kernel_environment() {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) return;
    
    for (int i = 0; i < 512; i++) buffer[i] = 0;

    int bytes = fs_read_file("/conf/env.txt", buffer, 511);
    if (bytes > 0) {
        char* text = (char*)buffer;
        if (starts_with(text, "PATH=")) { // Asigură-te că ai funcția starts_with sau o verifici manual
            // Copiem PATH-ul
            int i = 0;
            while (text[5 + i] != '\0' && i < 127) {
                env_path[i] = text[5 + i];
                i++;
            }
            env_path[i] = '\0';
            
            // Curățăm caracterele newline
            for (int j = 0; env_path[j] != '\0'; j++) {
                if (env_path[j] == '\n' || env_path[j] == '\r') {
                    env_path[j] = '\0';
                    break;
                }
            }
            print("Kernel: PATH setat la: ");
            print(env_path);
            newline();
        }
    } else {
        print("Kernel: /conf/env.txt negasit. Folosesc PATH implicit (/;/bin).");
        newline();
    }
}

void kernel_main(unsigned long magic, unsigned long addr) {
    cursor_init();
    print("Nano OS booted!");
    newline();

    // Verificăm și parsăm fișierul de configurare trimis prin GRUB
    if (magic == 0x2BADB002 && addr != 0) {
        multiboot_info_t* mb_info = (multiboot_info_t*)addr;

        // Verificăm dacă bitul 12 este setat (dacă există informații despre framebuffer)
        if (mb_info->flags & (1 << 12)) {
            uint64_t fb_addr = mb_info->framebuffer_addr;
            uint32_t fb_width = mb_info->framebuffer_width;
            uint32_t fb_height = mb_info->framebuffer_height;
            uint32_t fb_pitch = mb_info->framebuffer_pitch;
            uint8_t  fb_bpp = mb_info->framebuffer_bpp;

            // Aici poți salva aceste valori în variabile globale ale kernelului 
            // (ex: lfb_memory = (uint32_t*)fb_addr; screen_width = fb_width; etc.)
        }

        if (mb_info->mods_count > 0) {
            // Conversie sigură prin uint64_t pentru a evita warning-ul pe 64-biți
            mod_list_t* mod = (mod_list_t*)(uint64_t)mb_info->mods_addr;
            parse_config((const char*)(uint64_t)mod->mod_start);
        }
    }

    // Inițializăm întreruperile (inclusiv tastatura)
    interrupts_init();
    memory_init();
    fs_init();
    // Lansăm shell-ul interactiv direct
    
    // Încărcăm variabilele de mediu (PATH)
    load_kernel_environment();

    // clear_screen();
    //shell_init();
    //shell_run();
    //timer_init(1000);


    // --- LANSAREA SHELL-ULUI DIN USER SPACE ---
    print("Se incarca Shell-ul din User Space (/bin/sh)...\n");
    
    uint8_t* shell_memory = (uint8_t*)0x800000;
    int bytes = fs_read_file("/bin/sh", shell_memory, 32768);
    
    // Definiția structurii header-ului (trebuie să fie vizibilă sau definită și în kernel)
    typedef struct {
        char magic[4];       // "NAS1"
        uint32_t entry_offset;
    } __attribute__((packed)) NanoHeader;

    if (bytes > (int)sizeof(NanoHeader)) {
        NanoHeader* hdr = (NanoHeader*)shell_memory;
        
        // Verificăm semnătura magică "NAS1"
        if (hdr->magic[0] == 'N' && hdr->magic[1] == 'A' && 
            hdr->magic[2] == 'S' && hdr->magic[3] == '1') {
            
            print("Shell incarcat cu succes! Se ruleaza...\n");
            
            // Sărim exact peste header, la adresa de început a codului
            void (*shell_entry)(void) = (void (*)(void))(shell_memory + hdr->entry_offset);
            shell_entry();
        } else {
            print("EROARE CRITICA: /bin/sh nu are semnatura valida NAS1!\n");
        }
    } else {
        print("EROARE CRITICA: Nu s-a putut gasi /bin/sh pe disc sau este prea mic!\n");
    }
    // ------------------------------------------

    // Fallback de siguranță (în caz că shell-ul s-ar opri vreodată)
    for (;;) {
        __asm__ volatile ("hlt");
    }
}