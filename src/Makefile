# Toolset-ul folosit (GCC/LD pe 64-biți freestanding)
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -mno-red-zone -ffreestanding -fno-pie -c -Wall -Wextra
ASFLAGS = -f elf64

# --- MAGIA PAGINĂRII ---
TEXT_ADDR = 0x800000
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

# Găsim automat toate fișierele .c din directorul src (excepție făcând poate fișierele de librărie dacă sunt aici, dar le putem filtra)
# Dacă nano_libc.c și nano_string.c sunt aici, ele nu trebuie să aibă main(), deci le excludem din BINS:
SRCS = $(filter-out nano_libc.c nano_string.c, $(wildcard *.c))
BINS = $(patsubst %.c, %.bin, $(SRCS))

all: $(BINS)
	@echo "========================================="
	@echo " Toate aplicațiile din src au fost"
	@echo " compiliate cu succes în fișiere .bin! "
	@echo "========================================="

# 1. Compilare C Runtime Zero
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librării comune
nano_libc.o: nano_libc.c nano_libc.h
	$(CC) $(CFLAGS) nano_libc.c -o nano_libc.o 

nano_string.o: nano_string.c nano_string.h
	$(CC) $(CFLAGS) nano_string.c -o nano_string.o 

# 3. Regulă generală pentru compilarea aplicațiilor în .o
%.o: %.c nano_libc.h nano_string.h
	$(CC) $(CFLAGS) $< -o $@

# 4. Regulă generală de link final (fără push pe disc)
%.bin: %.o crt0.o nano_libc.o nano_string.o
	$(LD) $(LDFLAGS) crt0.o nano_libc.o nano_string.o $< -o $@

clean:
	rm -f *.o *.bin