# ============================
#   Kernel x86-64 – Makefile
# ============================

CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -nostdlib -T linker.ld

BOOT = boot/multiboot.asm
KERNEL = kernel/kernel.c

OBJS = boot.o kernel.o

all: kernel.bin

boot.o: $(BOOT)
	$(AS) -f elf64 $(BOOT) -o boot.o

kernel.o: $(KERNEL)
	$(CC) $(CFLAGS) -c $(KERNEL) -o kernel.o

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.bin

# ============================
#   ISO cu GRUB
# ============================

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
    # grub.cfg trebuie să existe deja în iso/boot/grub/
    # deci NU îl copiem peste el

kernel.iso: iso/boot/kernel.bin
	grub-mkrescue -o kernel.iso iso

# ============================
#   Rulare în QEMU
# ============================

run: kernel.iso
	qemu-system-x86_64 -cdrom kernel.iso

# ============================
#   Curățare
# ============================

clean:
	rm -f *.o kernel.bin kernel.iso
	rm -rf iso/boot/kernel.bin

