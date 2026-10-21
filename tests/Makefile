# --- Toolset-ul folosit ---
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -mno-red-zone -ffreestanding -fno-pie -c -Wall -Wextra -nostdinc -I../src -I../thirdparty/tinycc-mirror-repository/sys_stubs -I$(shell gcc -print-file-name=include)
ASFLAGS = -f elf64

TEXT_ADDR = 0x800000
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

# Căile și obiectele separate corect
SRC_DIR = ../src
LIBC_O  = nano_libc.o
STR_O   = nano_string.o
GFX_O   = libgfx.o
FONT_O  = gfx_font1.o
DS_O    = libds.o

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

# 2. Compilare librării comune și module din directorul ../src
$(LIBC_O): $(SRC_DIR)/nano_libc.c $(SRC_DIR)/nano_libc.h 
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_libc.c -o $(LIBC_O) 

$(STR_O): $(SRC_DIR)/nano_string.c $(SRC_DIR)/nano_string.h 
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_string.c -o $(STR_O) 
	
$(GFX_O): $(SRC_DIR)/libgfx.c $(SRC_DIR)/libgfx.h $(SRC_DIR)/gfx_font1.h
	$(CC) $(CFLAGS) $(SRC_DIR)/libgfx.c -o $(GFX_O)

$(FONT_O): $(SRC_DIR)/gfx_font1.c $(SRC_DIR)/gfx_font1.h
	$(CC) $(CFLAGS) $(SRC_DIR)/gfx_font1.c -o $(FONT_O)

$(DS_O): $(SRC_DIR)/libds.c $(SRC_DIR)/libds.h $(SRC_DIR)/nano_libc.h
	$(CC) $(CFLAGS) $(SRC_DIR)/libds.c -o $(DS_O)
	
# 3. Regulă generală pentru compilarea oricărui fișier .c local în .o
%.o: %.c $(SRC_DIR)/nano_libc.h $(SRC_DIR)/nano_string.h $(SRC_DIR)/libgfx.h $(SRC_DIR)/gfx_font1.h $(SRC_DIR)/libds.h
	$(CC) $(CFLAGS) $< -o $@

# 4. Regulă generală de link incluzând toată stiva (libc, string, gfx, font, ds)
%.bin: %.o crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O)
	$(LD) $(LDFLAGS) crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O) $< -o $@

clean:
	rm -f *.o *.bin