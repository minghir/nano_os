OUTPUT_FORMAT("binary")
ENTRY(_start)
SECTIONS
{
    . = 0x800000;  /* (sau 0x800000 în shell.ld) */
    .header : { *(.header) }
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
}