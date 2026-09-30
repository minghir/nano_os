OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
	. = TEXT_ADDR;
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