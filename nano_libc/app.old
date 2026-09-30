OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x900000; /* (Folosește 0x800000 pentru shell.ld) */

    .header : { *(.header) }
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }

    /* Eliminăm orice metadate generate automat de GCC/Clang/GNU */
    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame*)
    }
}