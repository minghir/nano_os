OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x800000; /* ACEASTA ESTE NOUA ADRESĂ FIXĂ */
    
    .text : {
        *(.text)
    }
    .data : {
        *(.data)
        *(.rodata*) /* Aici stau string-urile constante (ex: "Cum te numesti?") */
    }
    .bss : {
        *(.bss)
        *(COMMON)
    }
}