# Toolset-ul folosit (folosim GCC/LD pe 64-biți freestanding)
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-pie -c -Wall -Wextra
ASFLAGS = -f elf64

# Poți schimba aplicația implicită aici sau din consolă (ex: make APP=test_malloc)
APP ?= app

# Selectăm dinamic linker script-ul în funcție de aplicație
ifeq ($(APP),shell)
    LDFLAGS = -T shell.ld
else
    LDFLAGS = -T app.ld
endif

all: $(APP).bin

# 1. Compilare C Runtime Zero
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librărie nano_libc
nano_libc.o: nano_libc.c nano_libc.h
	$(CC) $(CFLAGS) nano_libc.c -o nano_libc.o

# 3. Compilare aplicație dinamică (funcționează pentru orice .c dai ca parametru)
$(APP).o: $(APP).c nano_libc.h
	$(CC) $(CFLAGS) $(APP).c -o $(APP).o

# 4. Link final în binar pur (folosește LDFLAGS dinamic)
$(APP).bin: crt0.o nano_libc.o $(APP).o
	$(LD) $(LDFLAGS) crt0.o nano_libc.o $(APP).o -o $(APP).bin
	../nanohdd ../hda.img push $(APP).bin /bin/$(APP)
	@echo "Succes! S-a creat binarul: $(APP).bin"

# Curățarea fișierelor oprite la compilare
clean:
	rm -f *.o *.bin