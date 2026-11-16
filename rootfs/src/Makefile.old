# --- Toolset-ul folosit ---
CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -mno-red-zone -ffreestanding -fno-pie -c -Wall -Wextra
ASFLAGS = -f elf64

# --- MAGIA PAGINĂRII ---
TEXT_ADDR = 0x800000
LDFLAGS = -T app.ld --defsym TEXT_ADDR=$(TEXT_ADDR)

# Excludem librăriile și fontul din lista de aplicații binare cu main()
SRCS = $(filter-out nano_libc.c nano_string.c libgfx.c gfx_font1.c libds.c, $(wildcard *.c))
BINS = $(patsubst %.c, %.bin, $(SRCS))

# Obiectele pentru librării și module
LIBC_O   = nano_libc.o
STR_O    = nano_string.o
GFX_O    = libgfx.o
FONT_O   = gfx_font1.o
DS_O     = libds.o

all: $(BINS)
	@echo "========================================="
	@echo " Toate aplicațiile din src au fost"
	@echo " compiliate cu succes în fișiere .bin! "
	@echo "========================================="

# 1. Compilare C Runtime Zero
crt0.o: crt0.asm
	$(AS) $(ASFLAGS) crt0.asm -o crt0.o

# 2. Compilare librării comune, module grafice și structuri de date
$(LIBC_O): nano_libc.c nano_libc.h
	$(CC) $(CFLAGS) nano_libc.c -o $(LIBC_O) 

$(STR_O): nano_string.c nano_string.h
	$(CC) $(CFLAGS) nano_string.c -o $(STR_O) 

$(GFX_O): libgfx.c libgfx.h gfx_font1.h
	$(CC) $(CFLAGS) libgfx.c -o $(GFX_O)

$(FONT_O): gfx_font1.c gfx_font1.h
	$(CC) $(CFLAGS) gfx_font1.c -o $(FONT_O)

$(DS_O): libds.c libds.h nano_libc.h
	$(CC) $(CFLAGS) libds.c -o $(DS_O)

# 3. Regulă generală pentru compilarea aplicațiilor în .o
%.o: %.c nano_libc.h nano_string.h libgfx.h libds.h
	$(CC) $(CFLAGS) $< -o $@

# 4. Regulă generală de link final incluzând toată stiva (libc, string, gfx, font, ds)
%.bin: %.o crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O)
	$(LD) $(LDFLAGS) crt0.o $(LIBC_O) $(STR_O) $(GFX_O) $(FONT_O) $(DS_O) $< -o $@

clean:
	rm -f *.o *.bin