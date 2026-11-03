#include <stdint.h>

#include "cpu.h"


void init_sse(void) {
    uint64_t cr0, cr4;

    // 1. Modificăm registrul CR0
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Ștergem bitul EM (Emulation) -> obligatoriu 0 pentru SSE
    cr0 |= (1 << 1);  // Setăm bitul MP (Monitor Coprocessor) -> recomandat
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    // 2. Modificăm registrul CR4
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Setăm OSFXSR (Activează suportul pentru FXSAVE / FXRSTOR)
    cr4 |= (1 << 10); // Setăm OSXMMEXCPT (Activează excepțiile SIMD pentru XMM)
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}


int check_sse_support(void) {
    uint32_t eax, ebx, ecx, edx;
    // Apelăm CPUID cu eax = 1
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    // Bitul 25 din EDX indică suportul pentru SSE
    return (edx & (1 << 25)) != 0;
}

int check_avx_support(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    // Bitul 28 din ECX indică suportul pentru AVX
    return (ecx & (1 << 28)) != 0;
}