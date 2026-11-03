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
#include "video.h"
#include "mouse.h"
#include "sys.h"
#include "drivers/cpu.h"
#include "auth.h"

#include "gfx/gfx_term.h"
#include "drivers/network/network.h"

#include <stdint.h>

extern int is_gfx_mode;
extern int desktop_mode;

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


uint64_t fb_physical_address = 0;
uint32_t screen_width = 0;
uint32_t screen_height = 0;
uint32_t screen_pitch = 0;
uint8_t  screen_bpp = 0;

extern volatile int mouse_moved;
extern void gfx_redraw_tty(int tty_id);
extern int active_tty;

extern void init_tss(void);

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

    KLOG_ERROR("\n--- PAGE FAULT DEBUG ---\n");
    KLOG_ERROR("CR2 (Memoria accesata gresit): ");
    print_number(faulting_address); // Poți folosi print_number temporar
    KLOG_ERROR("\nRIP (Unde a crapat codul): ");
    print_number(regs->rip);
    KLOG_ERROR("\n-------------------------\n");

    if (regs->rip >= 0x800000 && regs->rip < 0x900000) {
        KLOG_ERROR("-> RIP este in zona programului (0x800000)\n");
    } else {
        KLOG_ERROR("-> RIP este in afara zonei programului!\n");
    }
	

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


// Funcție pentru extragerea unui parametru (ex: "rootfs=") din linia de comandă a kernelului
void get_cmdline_param(const char* cmdline, const char* param_name, char* out_val, int max_len) {
    out_val[0] = '\0';
    if (!cmdline || !param_name) return;

    int param_len = string_length(param_name);
    int i = 0;

    while (cmdline[i] != '\0') {
        // Verificăm dacă șirul curent începe cu "param_name"
        int match = 1;
        for (int j = 0; j < param_len; j++) {
            if (cmdline[i + j] == '\0' || cmdline[i + j] != param_name[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            // Am găsit parametrul, copiem valoarea (până la spațiu sau sfârșit de șir)
            int k = i + param_len;
            int out_idx = 0;
            while (cmdline[k] != '\0' && cmdline[k] != ' ' && out_idx < max_len - 1) {
                out_val[out_idx++] = cmdline[k++];
            }
            out_val[out_idx] = '\0';
            return;
        }
        i++;
    }
}



void kernel_main(unsigned long magic, unsigned long addr) {
	init_sse();
    tty_init();
    cursor_init();
    KLOG_INFO("Nano OS booted!\n");

	process_init();
    interrupts_init();
    memory_init();

    // Verify and parse the configuration file sent via GRUB
    if (magic == 0x2BADB002 && addr != 0) {
        multiboot_info_t* mb_info = (multiboot_info_t*)addr;

        // Verificăm dacă GRUB ne-a dat framebuffer-ul grafic
        // În kernel.c, în interiorul verificării Multiboot:
		// Verificăm dacă GRUB ne-a dat framebuffer-ul grafic
        if ((mb_info->flags & (1 << 12)) && mb_info->framebuffer_addr != 0 && mb_info->framebuffer_type == 1) {
            fb_physical_address = mb_info->framebuffer_addr;
            screen_width  = mb_info->framebuffer_width;
            screen_height = mb_info->framebuffer_height;
            screen_pitch  = mb_info->framebuffer_pitch;

            // Mapăm memoria video pentru kernel în tabelele de pagini (PML4)
            uint32_t fb_size = screen_pitch * screen_height;
            uint64_t* current_pml4;
            __asm__ volatile("mov %%cr3, %0" : "=r"(current_pml4));
            for (uint32_t offset = 0; offset < fb_size; offset += PAGE_SIZE) {
                map_page(current_pml4,
                         fb_physical_address + offset,
                         fb_physical_address + offset,
                         PAGE_PRESENT | PAGE_WRITE);
            }

            // --- INIȚIALIZARE UNICĂ ȘI CORECTĂ A MOTORULUI GRAFIC ---
            video_init(fb_physical_address, screen_width, screen_height, screen_pitch, mb_info->framebuffer_bpp);

            VideoModeInfo* mode = video_get_info();
            //gterm_init(mode->fb_addr, mode->width, mode->height, mode->pitch);
			gterm_init((uint64_t)mode->back_buffer, mode->width, mode->height, mode->pitch);
            io_set_graphical_mode(mode->cols, mode->rows);
			desktop_mode = 0;
            is_gfx_mode = 1;
			mouse_init();
			
            gfx_redraw_tty(active_tty);
            // --------------------------------------------------------

            KLOG_INFO("KERNEL: Nano OS activat în Mod Grafic True Color (1024x768).\n");
        } else {
            KLOG_FATAL("FATAL: Framebuffer grafic negăsit de la GRUB!\n");
        }

        // Parsăm fișierul de configurare / timezone dacă există
        if (mb_info->mods_count > 0) {
            mod_list_t* mod = (mod_list_t*)(uint64_t)mb_info->mods_addr;
            parse_config((const char*)(uint64_t)mod->mod_start);
        }
    }


// Extragem linia de comandă a kernelului din structura Multiboot
    char kernel_cmdline[256] = {0};

    // Extragem cmdline din structura Multiboot
    if (magic == 0x2BADB002 && addr != 0) {
        multiboot_info_t* mb_info = (multiboot_info_t*)addr;
        
        // Verificăm flag-ul bitului 2 care indică prezența cmdline
        if (mb_info->flags & (1 << 2)) {
            const char* cmd = (const char*)(uint64_t)mb_info->cmdline;
            int i = 0;
            while (cmd[i] != '\0' && i < 255) {
                kernel_cmdline[i] = cmd[i];
                i++;
            }
            kernel_cmdline[i] = '\0';
            
            KLOG_INFO("KERNEL CMDLINE: ");
            KLOG_INFO(kernel_cmdline);
            KLOG_INFO("\n");
        }
    }

    KLOG_INFO("KERNEL:Try fs_init()\n");
    
    // Apelăm noua funcție fs_init pasând linia de comandă!
    fs_init(kernel_cmdline); 
    
    KLOG_INFO("KERNEL:fs_init() started\n");
  
  
    fs_create_file("/kernel.log", KERNEL_LOG_SIZE, 0644);
    KLOG_INFO("KERNEL:Log file created\n");

    load_kernel_environment();
    KLOG_INFO("KERNEL:Enviroment loaded.\n");
	
	KLOG_INFO("\nScanning PCI BUS...\n");
    pci_scan_bus();

	KLOG_INFO("\nLoading user table...\n");
	auth_init();

    KLOG_INFO("\nLoading Init daemons on all TTYs...\n");

//Apelează init_tss(); o singură dată în kernel_main, după ce ai inițializat GDT-ul, memoria și paging-ul, dar înainte să lansezi init-ul sau să activezi timerul.
	KLOG_INFO("[DEBUG] Incepem incarcarea INIT...\n");
    init_tss();
    KLOG_INFO("[DEBUG] TSS incarcat cu succes.\n");

    uint64_t* kernel_pml4;
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_pml4));

    for (int t = 0; t < 4; t++) {
        KLOG_INFO("[DEBUG] TTY: Creare PML4...\n");
        uint64_t init_pages[MAX_PROG_PAGES];
        uint64_t* init_pml4 = create_process_pml4();
        
        if (init_pml4 == 0) {
            KLOG_FATAL("FATAL: create_process_pml4 a returnat NULL!\n");
            while(1) __asm__ volatile("cli; hlt");
        }

		// ================= FIX: REDĂ VEDEREA KERNEL-ULUI =================
        // Mapăm memoria video (Framebuffer) și în PML4-ul User-Space-ului!
        uint32_t fb_size = screen_pitch * screen_height;
        for (uint32_t offset = 0; offset < fb_size; offset += PAGE_SIZE) {
            map_page(init_pml4, 
                     fb_physical_address + offset, 
                     fb_physical_address + offset, 
                     PAGE_PRESENT | PAGE_WRITE); // Doar Kernel-ul are voie să scrie (Fără PAGE_USER)
        }
        // =================================================================


        KLOG_INFO("[DEBUG] TTY: Alocare pagini fizice...\n");
        for (int p = 0; p < MAX_PROG_PAGES; p++) {
            init_pages[p] = (uint64_t)alloc_page();
            if (init_pages[p] == 0) KLOG_FATAL("alloc_page NULL!\n");
            map_page(init_pml4, 0x800000 + (p * 4096), init_pages[p], PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }

        KLOG_INFO("[DEBUG] TTY: Alocare temp_buffer prin malloc...\n");
        uint8_t* temp_buffer = (uint8_t*)malloc(MAX_PROG_PAGES * 4096);
        if (temp_buffer == 0) {
            KLOG_FATAL("FATAL: malloc a returnat NULL! Heap-ul este plin sau neinitializat corect.\n");
            while(1) __asm__ volatile("cli; hlt");
        }

        for (uint32_t i = 0; i < MAX_PROG_PAGES * 4096; i++) {
            temp_buffer[i] = 0;
        }

        KLOG_INFO("[DEBUG] TTY: Citire fs_read_file...\n");
        int bytes = fs_read_file("/hda/sbin/init", temp_buffer, MAX_PROG_PAGES * 4096);

        if (bytes > (int)sizeof(NanoHeader)) {
            KLOG_INFO("[DEBUG] TTY: Mapare si copiere in spatiul procesului...\n");
            for (int p = 0; p < MAX_PROG_PAGES; p++) {
                uint64_t safe_vaddr = 0x80000000 + (p * 4096); // 2 GB
                map_page(kernel_pml4, safe_vaddr, init_pages[p], PAGE_PRESENT | PAGE_WRITE);
                
                uint8_t* src = temp_buffer + (p * 4096);
                uint8_t* dest = (uint8_t*)safe_vaddr;
                for (int i = 0; i < 4096; i++) {
                    dest[i] = src[i];
                }
            }

            KLOG_INFO("[DEBUG] TTY: Apel process_create...\n");
            NanoHeader* hdr = (NanoHeader*)temp_buffer;
            uint64_t init_entry_point = 0x800000 + hdr->entry_offset;
            uint32_t init_pid = process_create("init", init_entry_point, 0, NULL, (uint64_t)init_pml4, init_pages, 0, t);

            if (init_pid == 0) {
                KLOG_FATAL("FATAL: process_create failed for init (process table or memory allocation failure)!\n");
                while(1) __asm__ volatile("cli; hlt");
            }
            
            KLOG_INFO("[DEBUG] TTY: Configurare PCB...\n");
            PCB* init_pcb = get_process_by_pid(init_pid);
            if (init_pcb == 0) {
                KLOG_FATAL("FATAL: process_create returned an unfindable init PID; check PCB layout consistency!\n");
                while(1) __asm__ volatile("cli; hlt");
            }
            init_pcb->cwd_sector = 0; 
            init_pcb->current_path[0] = '/';
            init_pcb->current_path[1] = '\0';
            
            ttys[t].foreground_pid = init_pid;
        } else {
            KLOG_FATAL("FATAL: /hda/sbin/init invalid sau missing!\n");
            while(1) __asm__ volatile("cli; hlt");
        }
        
        KLOG_INFO("[DEBUG] TTY: Eliberare temp_buffer...\n");
        free(temp_buffer);
    }
    
    KLOG_INFO("[DEBUG] Incarcare terminata. Pornim timer-ul!\n");
    // TTY-urile de la indexul 4 în sus rămân libere pentru daemoni de rețea (Telnet/SSH)
	KLOG_DEBUG("[DEBUG] 4 workspace TTYs created. Network TTYs 4+ reserved.\n");
	
	if(check_avx_support()) KLOG_INFO("AVX Support acctive!\n");
	if(check_sse_support()) KLOG_INFO("SSE Support acctive!\n");
	
	
	// Setăm starea procesului de kernel (PID 0) la SLEEPING sau ZOMBIE 
    // ca schedulerul să nu încerce niciodată să se întoarcă pe stiva de boot.
    if (current_process && current_process->pid == 0) {
        current_process->state = PROC_SLEEPING; 
    }
	
    timer_init(1000);
	
	__asm__ volatile ("sti");

    // Nu mai avem nevoie de bucla goală cu hlt! 
    // Lăsăm CPU-ul să ruleze libere procesele din tabelă.
    while (1) {
		
        __asm__ volatile ("hlt");
    }
}
