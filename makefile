# ============================
#    Kernel x86-64 – Makefile
# ============================

CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -mno-red-zone -O2 -Wall -Wextra
LDFLAGS = -m elf_x86_64 -nostdlib -T linker.ld

# 1. Găsește automat toate fișierele .c (în root și în folderul kernel/)
C_SOURCES = $(wildcard kernel/*.c)  \
			$(wildcard kernel/drivers/*.c) \
			$(wildcard kernel/drivers/sound/*.c) \
			$(wildcard kernel/drivers/sound/ac97/*.c) \
			$(wildcard kernel/fs/*.c) \
			$(wildcard kernel/gfx/*.c) \
			$(wildcard kernel/drivers/network/*.c) \
			$(wildcard kernel/drivers/network/e1000/*.c) \
			thirdparty/uip/uip/uip.c \
            thirdparty/uip/uip/uip_arp.c \
            thirdparty/uip/uip/timer.c
			
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
OBJS = boot.o isr_keyboard.o isr_timer.o isr_syscall.o isr_fault.o isr_mouse.o isr_e1000.o $(C_OBJS)

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
	
# Tinte helper pentru a forța reasamblarea corectă a boot.o
boot_gfx.o: boot/multiboot_gfx.asm
	$(AS) -f elf64 boot/multiboot_gfx.asm -o boot.o

boot_txt.o: boot/multiboot_txt.asm
	$(AS) -f elf64 boot/multiboot_txt.asm -o boot.o

# Regulă automată pentru fișierele .asm din root (ex: isr_keyboard.asm, timer.asm)
%.o: %.asm
	$(AS) -f elf64 $< -o $@

boot.o: boot/multiboot_gfx.asm
	$(AS) -f elf64 boot/multiboot_gfx.asm -o boot.o

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
	
isr_e1000.o: asm/isr_e1000.asm
	$(AS) -f elf64 asm/isr_e1000.asm -o isr_e1000.o


# Regulă automată pentru fișierele C din thirdparty/uip/
thirdparty/uip/uip/%.o: thirdparty/uip/uip/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Linkarea kernel-ului
kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.bin

# ============================
#    Tools (nan2hdd / nan3hdd)
# ============================

hdd_tools:
	$(MAKE) -C tools
	
tests: hdd_tools
	$(MAKE) -C tests
		
apps: hdd_tools tests
	$(MAKE) -C src
	
games: hdd_tools tests apps 
	$(MAKE) -C games/doom
	
# ============================
#    ISO cu GRUB
# ============================

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	
kernel.iso: iso/boot/kernel.bin hdd_tools
	grub-mkrescue -o kernel.iso iso
	#
	
img_files:
	qemu-img create -f raw hda_v3.img 50M
	qemu-img create -f raw hdb_v3.img 50M
# ============================
#    Rulare în QEMU
# ============================

run: run_txt

# Regula pentru Modul Grafic (GFX)
run_gfx: boot_gfx.o $(OBJS) kernel.iso hdd_tools apps tests games
	./init_hdd.sh
	qemu-system-x86_64 -cdrom kernel.iso \
		-drive file=hda_v3.img,format=raw,index=0,media=disk \
		-drive file=hdb_v3.img,format=raw,index=1,media=disk \
		-vga std \
		-net nic,model=e1000 \
		-net user,hostfwd=tcp::8080-:8080,hostfwd=tcp::2323-:23 \
		-display sdl \
		-cpu qemu64,+sse,+sse2,+avx 

# Regula pentru Modul Text (TXT)
run_txt: boot_txt.o $(OBJS) kernel.iso hdd_tools apps tests games
	./init_hdd.sh
	qemu-system-x86_64 -cdrom kernel.iso \
		-drive file=hda_v3.img,format=raw,index=0,media=disk \
		-drive file=hdb_v3.img,format=raw,index=1,media=disk \
		-net nic,model=e1000 \
		-net user,hostfwd=tcp::8080-:8080,hostfwd=tcp::2323-:23 \
		-display curses \
		-cpu qemu64,+sse,+sse2,+avx
		
build: boot_gfx.o $(OBJS) kernel.iso hdd_tools apps tests
	./init_hdd.sh
	
#run: kernel.iso hdd_tools apps 
#	qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v3.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -display sdl
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v3.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk  -audiodev sdl,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -vga std -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0
	#qemu-system-x86_64 -cdrom kernel.iso -drive file=hda_v2.img,format=raw,index=0,media=disk -drive file=hdb_v3.img,format=raw,index=1,media=disk -vga std -audiodev sdl,id=snd0 -device AC97,audiodev=snd0 -machine pcspk-audiodev=snd0 -display curses




# ============================
#    Curățare
# ============================

clean:
	$(MAKE) -C src clean
	$(MAKE) -C tests clean
	rm -f *.o kernel/*.o boot/*.o asm/*.o kernel.bin kernel.iso *.bin kernel/fs/*.o
	rm -f kernel/drivers/sound/*.o kernel/drivers/sound/ac97/*.o kernel/gfx/*.o
	rm -f thirdparty/uip/uip/*.o
	rm -f games/doom/*.o games/doom/*.bin
	rm -rf iso/boot/kernel.bin
	rm -f tools/nan2hdd tools/nan3hdd
	rm -f hda_v3.img hdb_v3.img