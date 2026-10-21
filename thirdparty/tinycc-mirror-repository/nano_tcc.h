// nano_tcc.h - Bridge între TCC și Nano OS
#ifndef NANO_TCC_H
#define NANO_TCC_H

#include "nano_libc.h"

// --- MEMORIE ---
#define malloc(sz)        nano_malloc(sz)
#define free(ptr)         nano_free(ptr)
#define realloc(ptr, sz)  nano_realloc(ptr, sz) // Asigură-te că ai realloc în nano_libc!

// --- FIȘIERE (Mapare peste EXT2 din Nano OS) ---
typedef struct {
    uint32_t inode;
    uint32_t offset;
    uint32_t size;
    uint8_t  is_open;
} FILE;

#define stdout ((FILE*)1)
#define stderr ((FILE*)2)
#define stdin  ((FILE*)0)

FILE* fopen(const char* path, const char* mode) {
    // Folosește funcțiile tale EXT2/syscalls pentru deschidere
    uint32_t ino = ext2_find_file(path);
    if (!ino && mode[0] == 'w') {
        ext2_create_file(path, 0);
        ino = ext2_find_file(path);
    }
    if (!ino) return NULL;

    FILE* f = (FILE*)malloc(sizeof(FILE));
    f->inode = ino;
    f->offset = 0;
    f->size = ext2_get_file_size(ino);
    f->is_open = 1;
    return f;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !stream->is_open) return 0;
    uint32_t bytes_to_read = size * nmemb;
    uint32_t bytes_read = ext2_read_file_offset(stream->inode, stream->offset, bytes_to_read, (uint8_t*)ptr);
    stream->offset += bytes_read;
    return bytes_read / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (stream == stdout || stream == stderr) {
        // Dacă TCC încearcă să scrie la stdout/stderr, afișăm pe ecran
        char temp[256];
        uint32_t total = size * nmemb;
        if (total > 255) total = 255;
        memcpy(temp, ptr, total);
        temp[total] = '\0';
        nano_print(temp);
        return nmemb;
    }
    if (!stream || !stream->is_open) return 0;
    uint32_t bytes = size * nmemb;
    ext2_write_file_offset(stream->inode, stream->offset, bytes, (uint8_t*)ptr);
    stream->offset += bytes;
    return nmemb;
}

int fclose(FILE* stream) {
    if (stream && stream != stdout && stream != stderr) {
        stream->is_open = 0;
        free(stream);
    }
    return 0;
}

// --- PRINTF / STRING ---
#define printf(...) nano_printf(__VA_ARGS__)
#define sprintf     snprintf

#endif