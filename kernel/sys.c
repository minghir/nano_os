#include <stdint.h>

#include "sys.h"
#include "io.h"
#include "process.h"
#include "memory.h"
#include "paging.h"
#include "tty.h"

void kernel_reboot() {
    print("System rebooting...\n");
    
    // 1. Încercăm resetul prin controlerul de tastatură
    outb(0x64, 0xFE);
    
    // 2. Metoda infailibilă pe x86: Triple Fault (Forțează resetul hardware)
    __asm__ volatile ("cli"); // Oprim întreruperile
    
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_zero = { 0, 0 };

    // Încărcăm un IDT complet gol (limită 0)
    __asm__ volatile ("lidt %0" :: "m"(idt_zero));
    
    // Generăm o excepție intenționată (breakpoint / int 3). 
    // Deoarece IDT-ul e gol, procesorul va genera un Fault, apoi un Double Fault, 
    // și în final un Triple Fault care va reseta instant QEMU!
    __asm__ volatile ("int $3");

    // 3. Plasă de siguranță supremă (dacă emularea ignoră triple fault-ul)
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void kill_process_by_pid(uint32_t pid) {
    __asm__ volatile ("cli");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            uint32_t ppid = process_table[i].ppid;
            int tty_id = process_table[i].tty_id;

            // 1. Eliberăm stiva alocată cu malloc (Curățăm resursele copilului mai întâi)
            if (process_table[i].stack_base) {
                free((void*)process_table[i].stack_base);
                process_table[i].stack_base = 0;
            }
			
			 __asm__ volatile ("mov %0, %%cr3" :: "r"(kernel_cr3));
			
            // 2. Eliberăm paginile fizice și paginarea (PML4)
            for (int p = 0; p < MAX_PROG_PAGES; p++) {
                if (process_table[i].prog_pages[p]) {
                    free_page((void*)process_table[i].prog_pages[p]);
                    process_table[i].prog_pages[p] = 0;
                }
            }
            if (process_table[i].cr3) {
                free_process_paging(process_table[i].cr3);
                process_table[i].cr3 = 0;
            }

            // 3. Eliberăm slotul procesului
            process_table[i].state = PROC_FREE;
            process_table[i].pid = 0;
            process_table[i].name[0] = '\0';

            // 4. Trezim părintele (după ce copilul a fost complet șters din sistem)
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (process_table[j].pid == ppid) {
                    if (process_table[j].state == PROC_SLEEPING) {
                        process_table[j].state = PROC_READY;
                        keyboard_flush();
                    }
                    break;
                }
            }

            // 5. Redăm foreground-ul părintelui pe TTY
            ttys[tty_id].foreground_pid = ppid;

            break;
        }
    }
}

/*
void kill_process_by_pid(uint32_t pid) {
    __asm__ volatile ("cli");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            uint32_t ppid = process_table[i].ppid;
            int tty_id = process_table[i].tty_id;

            // 1. Trezim părintele (care poate aștepta în sys_wait)
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (process_table[j].pid == ppid) {
                    process_table[j].state = PROC_READY;
                    break;
                }
            }

            // 2. Eliberăm stiva alocată cu malloc
            if (process_table[i].stack_base) {
                free((void*)process_table[i].stack_base);
                process_table[i].stack_base = 0;
            }

            // 3. Eliberăm paginile fizice și paginarea (PML4)
            for (int p = 0; p < 8; p++) {
                if (process_table[i].prog_pages[p]) {
                    free_page((void*)process_table[i].prog_pages[p]);
                    process_table[i].prog_pages[p] = 0;
                }
            }
            if (process_table[i].cr3) {
                free_process_paging(process_table[i].cr3);
                process_table[i].cr3 = 0;
            }

            // 4. Eliberăm slotul procesului
            process_table[i].state = PROC_FREE;
            process_table[i].pid = 0;
            process_table[i].name[0] = '\0';

            // 5. IMPORTANT: Părintele își recuperează instant controlul ecranului!
            ttys[tty_id].foreground_pid = ppid;

            break;
        }
    }
}
*/