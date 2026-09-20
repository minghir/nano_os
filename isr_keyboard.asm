global isr_keyboard
extern keyboard_irq

section .text
isr_keyboard:
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi

    call keyboard_irq

    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax

    mov al, 0x20
    out 0x20, al

    iretd

