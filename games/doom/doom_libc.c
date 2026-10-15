#include "../../src/nano_libc.h"
#include "../../src/nano_string.h"
#include "doomgeneric.h"
// Structură internă pentru a simula un "fișier" deschis în memorie
typedef struct {
    uint8_t* data;
    unsigned long size;
    unsigned long position;
    int is_wad;
} NanoFile;

static uint8_t* cached_wad_data = 0;
static unsigned long cached_wad_size = 0;

void init_wad_cache() {
    if (cached_wad_data) return;

    nano_print("[DEBUG] Aloc memorie pentru WAD cache (5MB)...\n");
    cached_wad_size = 5 * 1024 * 1024; // 5 MB
    cached_wad_data = nano_malloc(cached_wad_size);
    
    if (!cached_wad_data) {
        nano_print("[EROARE] Nu am putut aloca memorie pentru WAD cache!\n");
        return;
    }

    nano_print("[DEBUG] Incerc citirea DOOM1.WAD de pe disc...\n");
    int bytes_read = nano_read_file("DOOM1.WAD", cached_wad_data, cached_wad_size);
    if (bytes_read <= 0) {
        bytes_read = nano_read_file("doom1.wad", cached_wad_data, cached_wad_size);
    }
    if (bytes_read <= 0) {
        bytes_read = nano_read_file("/hda/games/doom/DOOM1.WAD", cached_wad_data, cached_wad_size);
    }
    if (bytes_read <= 0) {
        bytes_read = nano_read_file("/DOOM1.WAD", cached_wad_data, cached_wad_size);
    }
    
    if (bytes_read <= 0) {
        nano_print("[EROARE CRITICA] DOOM1.WAD nu a putut fi citit de pe disc! Bytes: 0\n");
    } else {
        // Afișăm numărul de bytes citiți folosind nano_print sau un mic format
        nano_print("[SUCCES] DOOM1.WAD citit in RAM cu succes!\n");
    }
}

void* fopen(const char* path, const char* mode) {
    if (!path) {
        nano_print("[fopen] EROARE: Path primit este NULL!\n");
        return 0;
    }

    nano_print("[fopen] Se incearca deschiderea fisierului: ");
    nano_print(path);
    nano_print("\n");

    if (string_contains(path, "WAD") || string_contains(path, "wad") || string_contains(path, "DOOM")) {
        init_wad_cache();
        if (!cached_wad_data) {
            nano_print("[fopen] EROARE: WAD cache este gol!\n");
            return 0;
        }

        NanoFile* file = nano_malloc(sizeof(NanoFile));
        if (!file) return 0;
        file->data = cached_wad_data;
        file->size = cached_wad_size;
        file->position = 0;
        file->is_wad = 1;
        return (void*)file;
    }

    // Pentru alte fișiere
    unsigned long max_sz = 65536; 
    uint8_t* temp_buf = nano_malloc(max_sz);
    if (!temp_buf) return 0;

    int bytes = nano_read_file(path, temp_buf, max_sz);
    if (bytes <= 0) {
        nano_print("[fopen] Fisierul secundar nu a fost gasit (returnez NULL): ");
        nano_print(path);
        nano_print("\n");
        nano_free(temp_buf);
        return 0; 
    }

    NanoFile* file = nano_malloc(sizeof(NanoFile));
    if (!file) {
        nano_free(temp_buf);
        return 0;
    }
    file->data = temp_buf;
    file->size = bytes;
    file->position = 0;
    file->is_wad = 0;
    return (void*)file;
}

unsigned long fread(void* ptr, unsigned long size, unsigned long nmemb, void* stream) {
    if (!stream) {
        nano_print("[fread] CRITICAL: S-a apelat fread cu stream NULL!\n");
        return 0;
    }
    NanoFile* file = (NanoFile*)stream;
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
        nano_print("[DEBUG] Inainte de doomgeneric_Tick...\n");
        doomgeneric_Tick();
        nano_print("[DEBUG] Dupa doomgeneric_Tick...\n");
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
int strcasecmp(const char *s1, const char *s2) { return strcmp(s1, s2); }
int strncasecmp(const char *s1, const char *s2, uint32_t n) { return strncmp(s1, s2, n); }

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
void exit(int status) { while(1); }
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
int printf(const char* format, ...) { return 0; }
int fprintf(void* stream, const char* format, ...) { return 0; }

// =========================================================
// 6. MINCIUNI PENTRU FIȘIERE (Doar ca să trecem de Linker)
// =========================================================

int fclose(void* stream) {
    if (!stream) return 0;
    NanoFile* file = (NanoFile*)stream;
    if (!file->is_wad) {
        if (file->data) nano_free(file->data);
        nano_free(file);
    } // Dacă e WAD, nu eliberăm memoria globală cache
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
int __printf_chk(int flag, const char *format, ...) { return 0; }
int __fprintf_chk(void* stream, int flag, const char *format, ...) { return 0; }
int __snprintf_chk(char *s, unsigned long maxlen, int flag, unsigned long os, const char *format, ...) { return 0; }
int __vfprintf_chk(void* stream, int flag, const char *format, void* ap) { return 0; }
int __vsnprintf_chk(char *s, unsigned long m, int f, unsigned long o, const char *fmt, void* ap) { return 0; }
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