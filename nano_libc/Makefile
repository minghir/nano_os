# Toolset-ul folosit (GCC/LD pe 64-biți freestanding)
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-pie -c -Wall -Wextra
ASFLAGS = -f elf64

# Aplicația curentă (ex: make APP=ps sau make APP=shell)
APP ?= app

# Atribuim adrese unice în memorie și căi pe disc pentru fiecare aplicație din build.sh
ifeq ($(APP),shell)
    TEXT_ADDR = 0x800000
    OS_PATH = /sbin/shell
else ifeq ($(APP),time)
    TEXT_ADDR = 0x810000
    OS_PATH = /sbin/time
else ifeq ($(APP),date)
    TEXT_ADDR = 0x820000
    OS_PATH = /sbin/date
else ifeq ($(APP),shutdown)
    TEXT_ADDR = 0x830000
    OS_PATH = /sbin/shutdown
else ifeq ($(APP),ls)
    TEXT_ADDR = 0x840000
    OS_PATH = /sbin/ls
else ifeq ($(APP),format)
    TEXT_ADDR = 0x850000
    OS_PATH = /sbin/format
else ifeq ($(APP),rm)
    TEXT_ADDR = 0x860000
    OS_PATH = /sbin/rm
else ifeq ($(APP),cat)
    TEXT_ADDR = 0x870000
    OS_PATH = /sbin/cat
else ifeq ($(APP),mkdir)
    TEXT_ADDR = 0x880000
    OS_PATH = /sbin/mkdir
else ifeq ($(APP),touch)
    TEXT_ADDR = 0x890000
    OS_PATH = /sbin/touch
else ifeq ($(APP),pwd)
    TEXT_ADDR = 0x8A0000
    OS_PATH = /sbin/pwd
else ifeq ($(APP),ps)
    TEXT_ADDR = 0x8B0000
    OS_PATH = /sbin/ps
else ifeq ($(APP),mandel)
    TEXT_ADDR = 0x900000
    OS_PATH = /bin/mandel
else ifeq ($(APP),nanoasm)
    TEXT_ADDR = 0x910000
    OS_PATH = /bin/asm
else ifeq ($(APP),nanovi)
    TEXT_ADDR = 0x920000
    OS_PATH = /bin/vi
else ifeq ($(APP),chr)
    TEXT_ADDR = 0xA00000
    OS_PATH = /tests/chr
else ifeq ($(APP),argt)
    TEXT_ADDR = 0xA10000
    OS_PATH = /tests/argt
else ifeq ($(APP),kill)
    TEXT_ADDR = 0x8C0000
    OS_PATH = /sbin/kill
else ifeq ($(APP),test_malloc)
    TEXT_ADDR = 0xA20000
    OS_PATH = /tests/test_malloc
else ifeq ($(APP),test_sleep)
    TEXT_ADDR = 0xA30000
    OS_PATH = /tests/test_sleep
else 
    # Valoare implicită pentru orice alt program nou
    TEXT_ADDR = 0xB00000
    OS_PATH = /bin/$(APP)
endif

# Folosim un singur app.ld general, iar adresa o forțăm cu -Ttext
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

all: $(APP).bin

# 1. Compilare C Runtime Zero
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librărie nano_libc
nano_libc.o: nano_libc.c nano_libc.h
	$(CC) $(CFLAGS) nano_libc.c -o nano_libc.o

# 3. Compilare aplicație
$(APP).o: $(APP).c nano_libc.h
	$(CC) $(CFLAGS) $(APP).c -o $(APP).o

# 4. Link final + urcare automată pe disc
$(APP).bin: crt0.o nano_libc.o $(APP).o
	$(LD) $(LDFLAGS) crt0.o nano_libc.o $(APP).o -o $(APP).bin
	../nanohdd ../hda.img push $(APP).bin $(OS_PATH)
	@echo "Succes! S-a creat și urcat: $(APP).bin la adresa $(TEXT_ADDR) -> $(OS_PATH)"

clean:
	rm -f *.o *.bin