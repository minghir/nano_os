# Toolset-ul folosit (GCC/LD pe 64-biți freestanding)
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-pie -c -Wall -Wextra
ASFLAGS = -f elf64

# Aplicația curentă (ex: make APP=ps sau make APP=shell)
APP ?= app

# --- MAGIA PAGINĂRII ---
# Acum TOATE programele se încarcă la exact aceeași adresă virtuală!
TEXT_ADDR = 0x800000

# Atribuim doar calea pe disc pentru utilitarul nanohdd (VFS)
ifeq ($(APP),shell)
    OS_PATH = /sbin/shell
else ifeq ($(APP),init)
    OS_PATH = /sbin/init	
else ifeq ($(APP),time)
    OS_PATH = /sbin/time
else ifeq ($(APP),date)
    OS_PATH = /sbin/date
else ifeq ($(APP),shutdown)
    OS_PATH = /sbin/shutdown
else ifeq ($(APP),ls)
    OS_PATH = /sbin/ls
else ifeq ($(APP),format)
    OS_PATH = /sbin/format
else ifeq ($(APP),rm)
    OS_PATH = /sbin/rm
else ifeq ($(APP),cat)
    OS_PATH = /sbin/cat
else ifeq ($(APP),mkdir)
    OS_PATH = /sbin/mkdir
else ifeq ($(APP),touch)
    OS_PATH = /sbin/touch
else ifeq ($(APP),pwd)
    OS_PATH = /sbin/pwd
else ifeq ($(APP),ps)
    OS_PATH = /sbin/ps
else ifeq ($(APP),kill)
    OS_PATH = /sbin/kill
else ifeq ($(APP),free)
    OS_PATH = /sbin/free
else ifeq ($(APP),cp)
    OS_PATH = /sbin/cp	
else ifeq ($(APP),mv)
    OS_PATH = /sbin/mv	
else ifeq ($(APP),df)
    OS_PATH = /bin/df
else ifeq ($(APP),dmesg)
    OS_PATH = /bin/dmesg	
else ifeq ($(APP),mandel)
    OS_PATH = /bin/mandel
else ifeq ($(APP),mp3_play)
    OS_PATH = /bin/mp3
else ifeq ($(APP),wavplay)
    OS_PATH = /bin/wav	
else ifeq ($(APP),nanoasm)
    OS_PATH = /bin/asm
else ifeq ($(APP),nanovi)
    OS_PATH = /bin/vi
else ifeq ($(APP),watch)
    OS_PATH = /bin/watch
else ifeq ($(APP),chr)
    OS_PATH = /tests/chr
else ifeq ($(APP),argt)
    OS_PATH = /tests/argt
else ifeq ($(APP),test_malloc)
    OS_PATH = /tests/test_malloc
else ifeq ($(APP),test_sleep)
    OS_PATH = /tests/test_sleep
else ifeq ($(APP),tsr_sleep2)
    OS_PATH = /tests/tsr_sleep2
else ifeq ($(APP),crash)
    OS_PATH = /tests/crash
else ifeq ($(APP),mem_test)
    OS_PATH = /tests/mem_test
else 
    # Valoare implicită pentru orice alt program nou
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
	
nano_string.o: nano_string.c nano_string.h
	$(CC) $(CFLAGS) nano_string.c -o nano_string.o 

# 3. Compilare aplicație
$(APP).o: $(APP).c nano_libc.h nano_string.h
	$(CC) $(CFLAGS) $(APP).c -o $(APP).o 

# 4. Link final + urcare automată pe disc
$(APP).bin: crt0.o nano_libc.o nano_string.o $(APP).o
	$(LD) $(LDFLAGS) crt0.o nano_libc.o nano_string.o $(APP).o -o $(APP).bin
	$(CURDIR)/../tools/nan2hdd $(CURDIR)/../hda_v2.img push $(APP).bin $(OS_PATH)
	@echo "Succes! S-a creat și urcat: $(APP).bin (Virtual: $(TEXT_ADDR)) -> $(OS_PATH)"

clean:
	rm -f *.o *.bin