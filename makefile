# ============================
#    Kernel x86-64 – Makefile
# ============================

CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -mno-red-zone -O2 -Wall -Wextra
LDFLAGS = -m elf_x86_64 -nostdlib -T linker.ld

# 1. Găsește automat toate fișierele .c (în root și în folderul kernel/)
C_SOURCES = $(wildcard kernel/*.c)  $(wildcard kernel/drivers/*.c) $(wildcard kernel/drivers/sound/*.c) $(wildcard kernel/drivers/sound/ac97/*.c) $(wildcard kernel/fs/*.c) $(wildcard kernel/gfx/*.c)
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
OBJS = boot.o isr_keyboard.o isr_timer.o isr_syscall.o isr_fault.o isr_mouse.o $(C_OBJS)

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

isr_timer.o: asm/isr_timer.asm
	$(AS) -f elf64 asm/isr_timer.asm -o isr_timer.o

isr_keyboard.o: asm/isr_keyboard.asm
	$(AS) -f elf64 asm/isr_keyboard.asm -o isr_keyboard.o

isr_syscall.o: asm/isr_syscall.asm
	$(AS) -f elf64 asm/isr_syscall.asm -o isr_syscall.o
	
isr_fault.o: asm/isr_fault.asm
	$(AS) -f elf64 asm/isr_fault.asm -o isr_fault.o
	
isr_mouse.o: asm/isr_mouse.asm
	$(AS) -f elf64 asm/isr_mouse.asm -o isr_mouse.o

# Linkarea kernel-ului
kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.bin

# ============================
#    Tools (nan2hdd / nan3hdd)
# ============================

hdd_tools:
	$(MAKE) -C tools
		
apps: hdd_tools
	$(MAKE) -C nano_libc APP=shell
	$(MAKE) -C nano_libc APP=init
	$(MAKE) -C nano_libc APP=time
	$(MAKE) -C nano_libc APP=date
	$(MAKE) -C nano_libc APP=shutdown
	$(MAKE) -C nano_libc APP=ls
	$(MAKE) -C nano_libc APP=format
	$(MAKE) -C nano_libc APP=rm
	$(MAKE) -C nano_libc APP=cat
	$(MAKE) -C nano_libc APP=mkdir
	$(MAKE) -C nano_libc APP=touch
	$(MAKE) -C nano_libc APP=cp
	$(MAKE) -C nano_libc APP=mv
	$(MAKE) -C nano_libc APP=df
	$(MAKE) -C nano_libc APP=pwd
	$(MAKE) -C nano_libc APP=ps
	$(MAKE) -C nano_libc APP=kill
	$(MAKE) -C nano_libc APP=free
	$(MAKE) -C nano_libc APP=dmesg
	$(MAKE) -C nano_libc APP=mandel
	$(MAKE) -C nano_libc APP=nanoasm
	$(MAKE) -C nano_libc APP=nanovi
	$(MAKE) -C nano_libc APP=watch
	$(MAKE) -C nano_libc APP=mp3_play
	$(MAKE) -C nano_libc APP=wavplay
	$(MAKE) -C nano_libc APP=chr
	$(MAKE) -C nano_libc APP=argt
	$(MAKE) -C nano_libc APP=test_malloc
	$(MAKE) -C nano_libc APP=test_sleep
	$(MAKE) -C nano_libc APP=tsr_sleep2
	$(MAKE) -C nano_libc APP=crash
	$(MAKE) -C nano_libc APP=mem_test

	
# ============================
#    ISO cu GRUB
# ============================

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	
kernel.iso: iso/boot/kernel.bin hdd_tools
	grub-mkrescue -o kernel.iso iso
	./init_hdd.sh
	
img_files:
	qemu-img create -f raw hda_v3.img 50M
	qemu-img create -f raw hdb_v3.img 50M
# ============================
#    Rulare în QEMU
# ============================

run: kernel.iso hdd_tools apps 
#	push_hda.sh
	qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v3.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -display sdl
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v3.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk  -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -vga std -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -vga std -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0 -display curses




# ============================
#    Curățare
# ============================

clean:
	rm -f *.o kernel/*.o boot/*.o src/*.o asm/*.o kernel.bin kernel.iso *.bin kernel/fs/*.o
	rm -f kernel/drivers/sound/*.o kernel/drivers/sound/ac97/*.o kernel/gfx/*.o
	rm -rf iso/boot/kernel.bin
	rm -f nano_libc/*.o nano_libc/*.bin
	rm -f tools/nan2hdd tools/nan3hdd
	rm -f hda_v3.img hdb_v3.img