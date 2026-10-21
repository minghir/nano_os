gcc -O2 -m64 -nostdlib -nostdinc -ffreestanding -c tcc.c -o tcc.o \
    -DCONFIG_TCC_TARG_X86_64=1 \
    -DCONFIG_TCC_BACKTRACE=0 \
    -Isys_stubs \
    -I. \
    -I../../src \
    -I$(gcc -print-file-name=include)


gcc -O2 -m64 -nostdlib -nostdinc -ffreestanding -c ../../src/nano_string.c -o nano_string.o \
    -Isys_stubs \
    -I. \
    -I../../src \
    -I$(gcc -print-file-name=include)

# 1. Asamblăm punctul de intrare crt0.asm
nasm -f elf64 crt0.asm -o crt0.o

# 2. Compilăm nano_libc.c (dacă nu este deja generat nano_libc.o)
gcc -O2 -m64 -nostdlib -nostdinc -ffreestanding -c ../../src/nano_libc.c -o nano_libc.o \
    -Isys_stubs \
    -I../../src \
    -I$(gcc -print-file-name=include)

# 3. Linkăm totul DIRECT în tcc.bin într-un singur pas
ld -m elf_x86_64 -T app.ld crt0.o tcc.o nano_libc.o nano_string.o -o tcc.bin