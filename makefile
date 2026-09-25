# ============================
#    Kernel x86-64 – Makefile
# ============================

CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -mno-red-zone -O2 -Wall -Wextra
LDFLAGS = -m elf_x86_64 -nostdlib -T linker.ld

# 1. Găsește automat toate fișierele .c (în root și în folderul kernel/)
C_SOURCES = $(wildcard src/*.c) $(wildcard kernel/*.c)
C_OBJS = $(C_SOURCES:.c=.o)

# 2. Găsește automat toate fișierele .asm (în root și în foldere precum boot/)
ASM_SOURCES = $(wildcard asm/*.asm) $(wildcard boot/*.asm)

# Convertim sursele asm în obiecte .o
# Notă: Pentru boot/multiboot.asm vrem să genereze boot.o direct în root
ASM_OBJS = $(notdir $(ASM_SOURCES:.asm=.o))
# Dacă boot/multiboot.asm devine multiboot.o prin regulă automată, 
# dar linkerul tău se așteaptă la boot.o, putem fie să redenumim fișierul asm în boot.asm, 
# fie să tratăm boot.asm separat. 
# Cel mai simplu: redenumim boot/multiboot.asm în boot/boot.asm sau tratăm excepția.

# Toate obiectele pentru linkare
OBJS = boot.o isr_keyboard.o timer_asm.o isr_syscall.o $(C_OBJS)

all: kernel.bin

# Regulă automată pentru fișierele C din root
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regulă automată pentru fișierele C din folderul kernel/
kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regulă automată pentru orice fișier .asm din folderul boot/
boot/%.o: boot/%.asm
	$(AS) -f elf64 $< -o $@

# Regulă automată pentru fișierele .asm din root (ex: isr_keyboard.asm, timer.asm)
%.o: %.asm
	$(AS) -f elf64 $< -o $@

boot.o: boot/multiboot.asm
	$(AS) -f elf64 boot/multiboot.asm -o boot.o

timer_asm.o: asm/timer.asm
	$(AS) -f elf64 asm/timer.asm -o timer_asm.o

isr_keyboard.o: asm/isr_keyboard.asm
	$(AS) -f elf64 asm/isr_keyboard.asm -o isr_keyboard.o

isr_syscall.o: asm/isr_syscall.asm
	$(AS) -f elf64 asm/isr_syscall.asm -o isr_syscall.o

# Linkarea kernel-ului
kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.bin

# ============================
#    ISO cu GRUB
# ============================

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin

kernel.iso: iso/boot/kernel.bin
	grub-mkrescue -o kernel.iso iso

# ============================
#    Rulare în QEMU
# ============================

run: kernel.iso
	qemu-system-x86_64 -cdrom kernel.iso -drive file=hda.img,format=raw,index=0,media=disk
	#qemu-system-x86_64 -kernel kernel.bin -drive file=hda.img,format=raw,index=0,media=disk -cpu qemu64
	#qemu-system-x86_64 -cdrom kernel.iso

# ============================
#    Curățare
# ============================

clean:
	rm -f *.o kernel/*.o boot/*.o kernel.bin kernel.iso *.bin
	rm -rf iso/boot/kernel.bin