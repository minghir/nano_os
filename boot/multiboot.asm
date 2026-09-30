section .multiboot
align 8
multiboot_header:
    dd 0x1BADB002        ; magic
    dd 0                 ; flags
    dd -(0x1BADB002)     ; checksum

section .text
global _start
extern kernel_main

BITS 32

_start:
    mov esp, stack_top
    and esp, 0xFFFFFFF0
    cld

    ; Salvăm parametrii primiți de la GRUB în registrele EAX și EBX
    mov [multiboot_magic], eax
    mov [multiboot_info_ptr], ebx

    call setup_page_tables
    call enable_long_mode

    ; Salt far în Long Mode folosind selectorul de cod 0x08 din GDT
    jmp 0x08:long_mode_start

setup_page_tables:
    ; 1. Leagă L4 la L3 (prima intrare)
    mov eax, page_table_l3
    or eax, 0x03             ; Present + Writable
    mov [page_table_l4], eax

    ; 2. Leagă L3 la L2 (prima intrare)
    mov eax, page_table_l2
    or eax, 0x03             ; Present + Writable
    mov [page_table_l3], eax

    ; 3. Mapează identity primii 18 MiB în pagini de 2 MiB.
    ; Heap-ul începe la 16 MiB și are 1 MiB, deci sunt necesare 9 intrări.
    xor ecx, ecx
.map_2mb:
    mov eax, ecx
    shl eax, 21
    or eax, 0x00000083       ; Present + Writable + Huge Page
    mov [page_table_l2 + ecx * 8], eax
    inc ecx
    cmp ecx, 9
    jne .map_2mb

    ret

enable_long_mode:
    ; Încarcă GDT
    lgdt [gdt_descriptor]

    ; Setează CR3 să indice spre PML4 (page_table_l4)
    mov eax, page_table_l4
    mov cr3, eax

    ; Activează PAE și suportul SSE necesar codului C optimizat
    mov eax, cr4
    or eax, (1 << 10) | (1 << 9) | (1 << 5)
    mov cr4, eax

    ; Activează Long Mode în MSR-ul EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8           ; LME (Long Mode Enable)
    wrmsr

    ; Activează paginarea în CR0 (ceea ce activează efectiv Long Mode)
    mov eax, cr0
    or eax, 1 << 31          ; PG (Paging)
    mov cr0, eax
    ret

BITS 64
long_mode_start:
    ; Setează registrele de date pentru 64-biți
    mov ax, 0x10             ; Selectorul pentru data segment din GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reconfigurează stiva pentru 64-biți
    mov rsp, stack_top
    and rsp, 0xFFFFFFFFFFFFFFF0

    ; Preluăm parametrii salvați și îi punem în rdi și rsi (Convenția System V AMD64)
    mov edi, [multiboot_magic]     ; Primul argument -> magic
    mov esi, [multiboot_info_ptr]  ; Al doilea argument -> addr

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss
alignb 16
stack_bottom:
    resb 16384
stack_top:

multiboot_magic: resq 1
multiboot_info_ptr: resq 1

alignb 4096
page_table_l4:
    resq 512
page_table_l3:
    resq 512
page_table_l2:
    resq 512

section .data
align 8
gdt_start:
    dq 0                     ; Null Descriptor
    dq 0x00AF9A000000FFFF    ; 64-bit code descriptor (R/X)
    dq 0x00CF92000000FFFF    ; 64-bit data descriptor (R/W)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start