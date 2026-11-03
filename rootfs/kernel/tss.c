#include "tss.h"
#include "syslog.h" // Pentru KLOG_INFO (sau include fișierul unde ai definit KLOG_INFO)
// Am mărit la 8 intrări (64 octeți) pentru a asigura padding-ul limitei hardware!
uint64_t kernel_gdt[8] __attribute__((aligned(16)));

tss_t kernel_tss = {0};

void init_tss(void) {
    KLOG_INFO("[TSS DEBUG] Pas 1: Setare GDT in memorie...\n");
    kernel_gdt[0] = 0;                        
    kernel_gdt[1] = 0x00AF9A000000FFFF;       
    kernel_gdt[2] = 0x00CF92000000FFFF;       
    kernel_gdt[3] = 0x00AFFA000000FFFF;       
    kernel_gdt[4] = 0x00CFF2000000FFFF;       

    KLOG_INFO("[TSS DEBUG] Pas 2: Configurare adrese TSS...\n");
    kernel_tss.iopb_offset = sizeof(tss_t);
    
    uint64_t tss_base = (uint64_t)&kernel_tss;
    // CRUCIAL: Am schimbat in uint64_t pentru a preveni Undefined Behavior in GCC la bitshift-ul << 48!
    uint64_t tss_limit = sizeof(tss_t) - 1; 

    KLOG_INFO("[TSS DEBUG] Pas 3: Scriere TSS in GDT...\n");
    kernel_gdt[5] = (tss_limit & 0xFFFF) | 
                    ((tss_base & 0xFFFFFF) << 16) | 
                    (0x89ULL << 40) | 
                    (((tss_limit >> 16) & 0xF) << 48) | 
                    (((tss_base >> 24) & 0xFF) << 56);
                    
    kernel_gdt[6] = (tss_base >> 32);
    kernel_gdt[7] = 0; // Padding de siguranta (Hardware Limit Bug fix)

    KLOG_INFO("[TSS DEBUG] Pas 4: Pregatire structura GDTR...\n");
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    
    gdtr.limit = sizeof(kernel_gdt) - 1; // Va fi 63 acum
    gdtr.base = (uint64_t)&kernel_gdt[0];

    KLOG_INFO("[TSS DEBUG] Pas 5: Apel hardware LGDT...\n");
    __asm__ volatile("lgdt %0" :: "m"(gdtr));

    KLOG_INFO("[TSS DEBUG] Pas 6: Apel hardware LTR (Incarcare Task Register)...\n");
    
    // Folosim o variabila sigura de C in locul specificatorului de AX
    uint16_t tr_selector = 0x28;
    __asm__ volatile("ltr %0" :: "r" (tr_selector));

    KLOG_INFO("[TSS DEBUG] Pas 7: Succes complet! TSS activat.\n");
}