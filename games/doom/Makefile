# ==========================================
# Makefile pentru DOOM pe Nano OS
# ==========================================

CC = gcc
AS = nasm
LD = ld

# Flag-uri combinate: Optimizare (-O2) + setări Nano OS + setări DOOM
CFLAGS = -O2 -m64 -mno-red-zone -ffreestanding -fno-pie -c -w -D__BIG_ENDIAN__=0 -I../../src -I.
ASFLAGS = -f elf64

TEXT_ADDR = 0x800000
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

# Căile către codul Nano OS
SRC_DIR = ../../src

# Obiectele sistemului de operare
LIBC_O  = nano_libc.o
STR_O   = nano_string.o
GFX_O   = libgfx.o
FONT_O  = gfx_font1.o
DS_O    = libds.o

# Găsim toate sursele DOOM
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRCS))

TARGET = doom.bin

all: $(TARGET)
	@echo "========================================="
	@echo " DOOM a fost compilat cu succes!"
	@echo "========================================="

# 1. Compilare C Runtime Zero (local)
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librării Nano OS (aduse din ../../src)
$(LIBC_O): $(SRC_DIR)/nano_libc.c
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_libc.c -o $(LIBC_O) 

$(STR_O): $(SRC_DIR)/nano_string.c
	$(CC) $(CFLAGS) $(SRC_DIR)/nano_string.c -o $(STR_O) 
	
$(GFX_O): $(SRC_DIR)/libgfx.c
	$(CC) $(CFLAGS) $(SRC_DIR)/libgfx.c -o $(GFX_O)

$(FONT_O): $(SRC_DIR)/gfx_font1.c
	$(CC) $(CFLAGS) $(SRC_DIR)/gfx_font1.c -o $(FONT_O)

$(DS_O): $(SRC_DIR)/libds.c
	$(CC) $(CFLAGS) $(SRC_DIR)/libds.c -o $(DS_O)

# 3. Compilare surse DOOM
%.o: %.c
	@echo "Compilam DOOM: $<..."
	$(CC) $(CFLAGS) $< -o $@

# 4. Linkare finală (crt0.o trebuie să fie PRIMUL ca să pună headerul NAS1 la offset 0)
$(TARGET): crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O) $(OBJS)
	@echo "=> Linkăm DOOM pentru Nano OS..."
	$(LD) $(LDFLAGS) crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O) $(OBJS) -o $@

clean:
	rm -f *.o $(TARGET)