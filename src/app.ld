OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
	. = 0x800000;
    .header : { *(.header) }
    .text   : { *(.text) }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.eh_frame*)
    }
}