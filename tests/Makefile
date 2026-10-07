# Toolset-ul folosit (GCC/LD pe 64-biți freestanding)
CC = gcc
AS = nasm
LD = ld

# Adăugăm -I../src ca compilatorul să știe unde să găsească headerele (.h) din src
CFLAGS = -m64 -mno-red-zone -ffreestanding -fno-pie -c -Wall -Wextra -I../src
ASFLAGS = -f elf64

TEXT_ADDR = 0x800000
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

# Căile către fișierele din ../src
SRC_DIR = ../src
LIBC_O = nano_libc.o
STR_O = nano_string.o

# Găsim automat toate fișierele .c din directorul curent (tests)
SRCS = $(wildcard *.c)
BINS = $(patsubst %.c, %.bin, $(SRCS))

all: $(BINS)
	@echo "========================================="
	@echo " Toate testele au fost compilate cu succes!"
	@echo "========================================="

# 1. Compilare C Runtime Zero (local)
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librării comune din directorul ../src
$(LIBC_O): $(SRC_DIR)/nano_libc.c $(SRC_DIR)/nano_libc.h
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_libc.c -o $(LIBC_O) 

$(STR_O): $(SRC_DIR)/nano_string.c $(SRC_DIR)/nano_string.h
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_string.c -o $(STR_O) 

# 3. Regulă generală pentru compilarea oricărui fișier .c local în .o
%.o: %.c $(SRC_DIR)/nano_libc.h $(SRC_DIR)/nano_string.h
	$(CC) $(CFLAGS) $< -o $@

# 4. Regulă generală de link pentru fiecare .bin (fără push pe disc)
%.bin: %.o crt0.o $(LIBC_O) $(STR_O)
	$(LD) $(LDFLAGS) crt0.o $(LIBC_O) $(STR_O) $< -o $@

clean:
	rm -f *.o *.bin