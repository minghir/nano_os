#include "nano_libc.h"

// Funcție inline de C care execută instrucțiunea de assembly CPUID
static inline void native_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

int main(int argc, char* argv[]) {
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];
    char brand[49];

    nano_print("\n=== Nano OS CPU Info ===\n");

    // 1. VENDOR ID (Aflăm producătorul: Intel, AMD, etc.)
    native_cpuid(0, &eax, &ebx, &ecx, &edx);
    
    // Numele vendorului e ascuns în EBX, EDX și ECX (exact în ordinea asta)
    ((uint32_t*)vendor)[0] = ebx;
    ((uint32_t*)vendor)[1] = edx;
    ((uint32_t*)vendor)[2] = ecx;
    vendor[12] = '\0';
    
    nano_print("Vendor ID  : ");
    nano_print(vendor);
    nano_print("\n");

    // 2. BRAND STRING (Numele complet și modelul procesorului)
    // Întâi verificăm dacă procesorul suportă interogarea de nume extins
    native_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        // Citim cele 3 bucăți de câte 16 octeți
        native_cpuid(0x80000002, (uint32_t*)(brand + 0),  (uint32_t*)(brand + 4),  (uint32_t*)(brand + 8),  (uint32_t*)(brand + 12));
        native_cpuid(0x80000003, (uint32_t*)(brand + 16), (uint32_t*)(brand + 20), (uint32_t*)(brand + 24), (uint32_t*)(brand + 28));
        native_cpuid(0x80000004, (uint32_t*)(brand + 32), (uint32_t*)(brand + 36), (uint32_t*)(brand + 40), (uint32_t*)(brand + 44));
        brand[48] = '\0';
        
        nano_print("Model Name : ");
        nano_print(brand);
        nano_print("\n");
    } else {
        nano_print("Model Name : [Indisponibil pe acest procesor]\n");
    }

    // 3. INFORMAȚII TEHNICE (Familie, Model, Stepping)
    native_cpuid(1, &eax, &ebx, &ecx, &edx);
    int stepping = eax & 0xF;
    int model = (eax >> 4) & 0xF;
    int family = (eax >> 8) & 0xF;
    
    nano_print("Family     : "); nano_print_int(family); nano_print("\n");
    nano_print("Model      : "); nano_print_int(model); nano_print("\n");
    nano_print("Stepping   : "); nano_print_int(stepping); nano_print("\n");

    nano_print("========================\n");

    return 0; // Se va întoarce curat în crt0.asm și va declanșa SYSCALL_EXIT
}