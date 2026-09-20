#include "../io.h"
#include "../config.h"
#include "../shell.h"
#include "../memory.h"
#include "../fs.h"
#include <stdint.h>

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct mod_list {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
} mod_list_t;

void kernel_main(unsigned long magic, unsigned long addr) {
    cursor_init();
    print("Nano OS booted!");
    newline();

    // Verificăm și parsăm fișierul de configurare trimis prin GRUB
    if (magic == 0x2BADB002 && addr != 0) {
        multiboot_info_t* mb_info = (multiboot_info_t*)addr;
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
    shell_init();
    shell_run();

    // Fallback de siguranță (în caz că shell-ul s-ar opri vreodată)
    for (;;) {
        __asm__ volatile ("hlt");
    }
}