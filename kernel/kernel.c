#include "io.h"
#include "config.h"
#include "process.h"
#include "memory.h"
#include "fs/fs.h"
#include "string.h"
#include "timer.h"
#include "paging.h"
#include "syslog.h"
#include "syscall.h"
#include "tty.h"
#include "pci.h"

#include <stdint.h>


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

char env_path[128] = "/hda/;/hda/sbin";

void load_kernel_environment() {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) return;
    
    for (int i = 0; i < 512; i++) buffer[i] = 0;

    int bytes = fs_read_file("/hda/cfg/env.cfg", buffer, 511);
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
            KLOG_INFO("KERNEL: PATH setat la: ");
            KLOG_INFO(env_path);
            KLOG_INFO("\n");
        }
    } else {
        KLOG_WARNING("KERNEL: /cfg/env.cfg missing. Default PATH (/;/bin).\n");
    }
}

// Structură specială doar pentru excepțiile care generează Error Code (ex: Page Fault)
typedef struct {
    uint8_t xmm0[16];
    uint64_t padding;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    
    uint64_t error_code;  // <--- ELEMENTUL LIPSĂ CARE DECALA TOTUL!
    
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) FaultRegisters;


// Schimbă argumentul din Registers* în FaultRegisters*
void page_fault_handler(FaultRegisters* regs) {
    uint64_t faulting_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(faulting_address));

    // Verificăm cine a crăpat cu adevărat analizând Adresa Instrucțiunii (RIP)
    // - Dacă RIP >= 0x800000 (Suntem în zona programelor)
    // - Dacă RIP < 0x100000  (S-a sărit la o adresă invalidă ca NULL / 0x0)
    // - Dacă CS indică Ring 3 (Pentru viitor, când vei implementa Ring 3)
	if (regs->cs & 3 || regs->rip >= 0x800000 || regs->rip < 0x100000) {
        // --- CRASH ÎN PROGRAM (User Space) ---
        KLOG_ERROR("\nKERNEL:[CRASH] Programul a generat Page Fault! (Memorie invalida)\n");
        
        if (current_process) {
            uint32_t ppid = current_process->ppid;

            // 1. TREZIM PĂRINTELE (Shell-ul) care a rămas blocat în SYSCALL_WAIT
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (process_table[i].pid == ppid) {
                    process_table[i].state = PROC_READY; 
                    break;
                }
            }

            // 2. Eliberăm slotul procesului curent
            current_process->state = PROC_FREE; 
            current_process->pid = 0;
            current_process->name[0] = '\0';
        }
        
        current_process = (PCB*)0; 
        
        // Așteptăm timer-ul să comute pe Shell-ul tocmai trezit
        while (1) {
            __asm__ volatile ("sti; hlt");
        }
    } else {
        // --- CRASH REAL ÎN KERNEL ---
        // Aici ajunge doar dacă ai un bug în funcțiile interne ale kernelului (ex: fs_read, malloc)
        KLOG_INFO("\nFATAL: KERNEL PAGE FAULT IN RING 0!\n");
        print(kernel_log_buffer);
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }
}

void kernel_main(unsigned long magic, unsigned long addr) {
	
	// 1. Inițializăm subsistemul TTY și curățăm ecranele virtuale
    tty_init();
	
    cursor_init();
    KLOG_INFO("Nano OS booted!\n");
   

    // Verify and parse the configuration file sent via GRUB
    if (magic == 0x2BADB002 && addr != 0) {
        multiboot_info_t* mb_info = (multiboot_info_t*)addr;

        // Check if bit 12 is set (framebuffer info available)
        if (mb_info->flags & (1 << 12)) {
            uint64_t fb_addr = mb_info->framebuffer_addr;
            uint32_t fb_width = mb_info->framebuffer_width;
            uint32_t fb_height = mb_info->framebuffer_height;
            uint32_t fb_pitch = mb_info->framebuffer_pitch;
            uint8_t  fb_bpp = mb_info->framebuffer_bpp;
            
            // Framebuffer values can be saved here if needed globally
        }

        if (mb_info->mods_count > 0) {
            mod_list_t* mod = (mod_list_t*)(uint64_t)mb_info->mods_addr;
            parse_config((const char*)(uint64_t)mod->mod_start);
        }
    }

    // Initialize core kernel subsystems and interrupts
    process_init();
    interrupts_init();
    memory_init();
	KLOG_INFO("KERNEL:Try fs_init()\n");
    fs_init();
	KLOG_INFO("KERNEL:fs_init() started\n");
	fs_create_file("/kernel.log", KERNEL_LOG_SIZE);
	KLOG_INFO("KERNEL:Log file created\n");
    
    // Load kernel environment variables (PATH)
    load_kernel_environment();
	KLOG_INFO("KERNEL:Enviroment loaded.\n");
	
	pci_scan_bus();
	// --- LOADING THE SHELL(S) FROM USER SPACE ---
    KLOG_INFO("Loading Init daemons on all TTYs...\n");
    
    // Salvăm cr3-ul curent al kernelului o singură dată
    uint64_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));

    // Pornim 4 shell-uri independente
    for (int t = 0; t < MAX_TTYS; t++) {
        
        uint64_t init_pages[8];
        for(int p = 0; p < 8; p++) {
            init_pages[p] = (uint64_t)alloc_page();
        }

        uint64_t* init_pml4 = create_process_pml4();

        for (int p = 0; p < 8; p++) {
            map_page(init_pml4, 0x800000 + (p * 4096), init_pages[p], 
                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }

        // Trecem pe memoria noului proces
        __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)init_pml4));

        // Citim fișierul direct în memoria fizică a ACESTUI proces
        int bytes = fs_read_file("/hda/sbin/init", (uint8_t*)0x800000, MAX_PROG_PAGES*4096);
        
        if (bytes > (int)sizeof(NanoHeader)) {
            NanoHeader* hdr = (NanoHeader*)0x800000;
            if (hdr->magic[0] == 'N' && hdr->magic[1] == 'A' && 
                hdr->magic[2] == 'S' && hdr->magic[3] == '1') {
                
                uint64_t init_entry_point = 0x800000 + hdr->entry_offset;
                
                // Revenim la kernel
                __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));
                
                // Transmitem parametrul `t` ca fiind tty_id-ul procesului!
               uint32_t init_pid = process_create("init", init_entry_point, 0, NULL, (uint64_t)init_pml4, init_pages, t);
			   ttys[t].foreground_pid = init_pid;
			   
            } else {
                __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));
                KLOG_FATAL("FATAL ERROR: /sbin/init bad signature!\n");
            }
        } else {
            __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));
            KLOG_FATAL("FATAL ERROR: /sbin/init missing!\n");
        }
    }

    KLOG_DEBUG("[DEBUG] 4 TTY processes created! Entering scheduler loop...\n");
    timer_init(1000); 
    
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}