#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// Structura TSS pentru x86_64 (Trebuie să fie exact formatul cerut de procesor)
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;      // <--- AICI SE UITĂ PROCESORUL LA ÎNTRERUPERI!
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

extern tss_t kernel_tss;

#endif