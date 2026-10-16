#include "../../src/nano_libc.h"
#include "../../src/nano_string.h"
#include "doomgeneric.h"
// Structură internă pentru a simula un "fișier" deschis în memorie
typedef struct {
    uint8_t* data;
    unsigned long size;
    unsigned long position;
} NanoFile;

static int is_doom1_wad_name(const char* path) {
    const char* name = path;
    const char* current = path;

    while (*current) {
        if (*current == '/' || *current == '\\') name = current + 1;
        current++;
    }

    static const char expected[] = "doom1.wad";
    for (unsigned int i = 0; i < sizeof(expected); i++) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != expected[i]) return 0;
    }
    return 1;
}

static int is_wad_name(const char* path) {
    unsigned int length = string_length(path);
    if (length < 4) return 0;

    const char* extension = path + length - 4;
    return extension[0] == '.' &&
           (extension[1] == 'w' || extension[1] == 'W') &&
           (extension[2] == 'a' || extension[2] == 'A') &&
           (extension[3] == 'd' || extension[3] == 'D');
}

void* fopen(const char* path, const char* mode) {
    if (!path || !mode || mode[0] == 'w' || mode[0] == 'a') return 0;

    unsigned long max_size = is_wad_name(path) ? 5 * 1024 * 1024 : 65536;
    uint8_t* data = nano_malloc(max_size);
    if (!data) return 0;

    int bytes = nano_read_file(path, data, (uint32_t)max_size);
    if (bytes <= 0 && is_doom1_wad_name(path)) {
        bytes = nano_read_file("/hdb/DOOM1.WAD", data, (uint32_t)max_size);
    }
    if (bytes <= 0) {
        nano_free(data);
        return 0;
    }

    NanoFile* file = nano_malloc(sizeof(NanoFile));
    if (!file) {
        nano_free(data);
        return 0;
    }
    file->data = data;
    file->size = bytes;
    file->position = 0;
    return (void*)file;
}

unsigned long fread(void* ptr, unsigned long size, unsigned long nmemb, void* stream) {
    if (!stream) {
        nano_print("[fread] CRITICAL: S-a apelat fread cu stream NULL!\n");
        return 0;
    }
    NanoFile* file = (NanoFile*)stream;
    if (size == 0) return 0;
    unsigned long total_to_read = size * nmemb;
    
    if (file->position + total_to_read > file->size) {
        total_to_read = file->size - file->position;
    }
    
    if (total_to_read == 0) return 0;

    for (unsigned long i = 0; i < total_to_read; i++) {
        ((uint8_t*)ptr)[i] = file->data[file->position + i];
    }
    
    file->position += total_to_read;
    return total_to_read / size;
}

int fseek(void* stream, long offset, int whence) {
    if (!stream) {
        nano_print("[fseek] CRITICAL: S-a apelat fseek cu stream NULL!\n");
        return -1;
    }
    NanoFile* file = (NanoFile*)stream;
    
    if (whence == 0) file->position = offset;
    else if (whence == 1) file->position += offset;
    else if (whence == 2) file->position = file->size + offset;
    
    if (file->position > file->size) file->position = file->size;
    return 0;
}


// =========================================================
// 1. ENTRY POINT-UL JOCULUI
// Aici aterizează codul tău din crt0.asm (apelul către main)
// =========================================================
int main(int argc, char** argv) {
    nano_print("=== NANO OS: PORNIRE MOTOR DOOM ===\n");
    
    nano_print("[DEBUG] Apelam doomgeneric_Create...\n");
    doomgeneric_Create(argc, argv);
    nano_print("[DEBUG] SUCCESS: doomgeneric_Create s-a terminat!\n");
    
    while (1) {
        doomgeneric_Tick();
        nano_yield();
    }
    return 0;
}

// Cerut de doomgeneric pentru titlul ferestrei (noi nu folosim)
void DG_SetWindowTitle(const char * title) {}

// =========================================================
// 2. MAPPING SPRE MEMORIA NANO OS
// =========================================================
void* malloc(unsigned long size) { return nano_malloc(size); }
void free(void* ptr) { nano_free(ptr); }
void* calloc(unsigned long num, unsigned long size) {
    void* ptr = nano_malloc(num * size);
    if (ptr) memset(ptr, 0, num * size);
    return ptr;
}
void* realloc(void* ptr, unsigned long new_size) {
    return nano_malloc(new_size); 
}

// =========================================================
// 3. MAPPING SPRE STRING-URILE TALE
// =========================================================
static unsigned char ascii_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c - 'A' + 'a') : c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && ascii_lower((unsigned char)*s1) == ascii_lower((unsigned char)*s2)) {
        s1++;
        s2++;
    }
    return ascii_lower((unsigned char)*s1) - ascii_lower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, uint32_t n) {
    while (n && *s1 && ascii_lower((unsigned char)*s1) == ascii_lower((unsigned char)*s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? ascii_lower((unsigned char)*s1) - ascii_lower((unsigned char)*s2) : 0;
}

char* strrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) { if (*s == c) last = s; s++; }
    return (char*)last;
}

char* strchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char*)s; s++; }
    return 0;
}

char* strdup(const char *s) {
    int len = string_length(s) + 1;
    char *dup = nano_malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

char* strncpy(char *dest, const char *src, unsigned long n) {
    unsigned long i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for ( ; i < n; i++) dest[i] = '\0';
    return dest;
}

char* strstr(const char *haystack, const char *needle) { return 0; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

// =========================================================
// 4. MATEMATICĂ ȘI SISTEM
// =========================================================
int abs(int j) { return j < 0 ? -j : j; }
double fabs(double x) { return x < 0.0 ? -x : x; }
double strtod(const char *nptr, char **endptr) { return 0.0; }
void exit(int status) {
    nano_print("\nDOOM: Se inchide...\n");

    // Dacă ai o funcție care readuce sistemul în modul text, apeleaz-o aici!
    // Dacă nu, măcar curățăm bufferul grafic ca să știm că a ieșit:
    if (DG_ScreenBuffer) {
        memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4); // Ecran negru
        nano_gfx_present(0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer);
    }

    register uint64_t rax_val __asm__("rax") = 20; // SYSCALL_EXIT
    register uint64_t rdi_val __asm__("rdi") = (uint64_t)status;
    
    __asm__ volatile (
        "int $0x80"
        : 
        : "r"(rax_val), "r"(rdi_val)
        : "cc", "memory"
    );
    while(1);
}
int system(const char* command) { return 0; }
int *__errno_location(void) { static int e = 0; return &e; }

// =========================================================
// 5. PRINTARE (Liniștim avertismentele consolei)
// =========================================================
void* stderr = (void*)2;
void* stdout = (void*)1;

int puts(const char *s) { nano_printf("%s\n", s); return 0; }
int putc(int c, void *stream) { return 0; }
int fflush(void *stream) { return 0; }
int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = nano_vprintf(format, args);
    va_end(args);
    return result;
}
int fprintf(void* stream, const char* format, ...) {
    (void)stream;
    va_list args;
    va_start(args, format);
    int result = nano_vprintf(format, args);
    va_end(args);
    return result;
}

// =========================================================
// 6. MINCIUNI PENTRU FIȘIERE (Doar ca să trecem de Linker)
// =========================================================

int fclose(void* stream) {
    if (!stream) return 0;
    NanoFile* file = (NanoFile*)stream;
    if (file->data) nano_free(file->data);
    nano_free(file);
    return 0;
}

unsigned long fwrite(const void* ptr, unsigned long size, unsigned long nmemb, void* stream) {
    return 0; // Read-only pentru moment, suficient pentru DOOM
}


long ftell(void* stream) {
    if (!stream) return -1;
    NanoFile* file = (NanoFile*)stream;
    return (long)file->position;
}


int remove(const char* pathname) { return -1; }
int rename(const char* oldpath, const char* newpath) { return -1; }
int mkdir(const char *pathname, unsigned int mode) { return -1; }

// =========================================================
// 7. HACK-URI PENTRU MIZERIILE COMPILATORULUI LINUX (_CHK)
// =========================================================
int __printf_chk(int flag, const char *format, ...) {
    (void)flag;
    va_list args;
    va_start(args, format);
    int result = nano_vprintf(format, args);
    va_end(args);
    return result;
}
int __fprintf_chk(void* stream, int flag, const char *format, ...) {
    (void)stream;
    (void)flag;
    va_list args;
    va_start(args, format);
    int result = nano_vprintf(format, args);
    va_end(args);
    return result;
}
int __snprintf_chk(char *s, unsigned long maxlen, int flag, unsigned long os, const char *format, ...) {
    (void)flag;
    (void)os;
    va_list args;
    va_start(args, format);
    int result = nano_vsnprintf(s, maxlen, format, args);
    va_end(args);
    return result;
}
int __vfprintf_chk(void* stream, int flag, const char *format, va_list args) {
    (void)stream;
    (void)flag;
    return nano_vprintf(format, args);
}
int __vsnprintf_chk(char *s, unsigned long size, int flag, unsigned long os, const char *format, va_list args) {
    (void)flag;
    (void)os;
    return nano_vsnprintf(s, size, format, args);
}
void *__memset_chk(void *dest, int c, unsigned long len, unsigned long destlen) { return memset(dest, c, len); }
void *__memcpy_chk(void *dest, const void *src, unsigned long len, unsigned long destlen) { return memcpy(dest, src, len); }
char *__strncpy_chk(char *dest, const char *src, unsigned long len, unsigned long destlen) { return strncpy(dest, src, len); }
long int __isoc23_strtol(const char *nptr, char **endptr, int base) { return 0; }
int __isoc23_sscanf(const char *str, const char *format, ...) { return 0; }
const unsigned short ** __ctype_b_loc(void) { static unsigned short* d=0; return (const unsigned short **)&d; }
const int ** __ctype_toupper_loc(void) { static int* d=0; return (const int **)&d; }


// =========================================================
// FUNCȚII LIPSĂ PENTRU STRCONV / SSCANF
// =========================================================
/*
long int strtol(const char *nptr, char **endptr, int base) {
    // O implementare simplă sau un stub care apelează ce ai nevoie
    long int val = 0;
    int sign = 1;
    while (*nptr == ' ' || *nptr == '\t') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') { nptr++; }
    while (*nptr >= '0' && *nptr <= '9') {
        val = val * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char*)nptr;
    return val * sign;
}

int __isoc99_sscanf(const char *str, const char *format, ...) {
    return 0; // Stub simplu ca să treacă de linker
}
*/