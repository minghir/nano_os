#ifndef SYS_MMAN_H
#define SYS_MMAN_H

#include "stdint.h"
#include "sys/types.h"

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void*)-1)

static inline void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)offset;
    return MAP_FAILED;
}

static inline int munmap(void* addr, size_t len) { (void)addr; (void)len; return 0; }
static inline int mprotect(void* addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }

#endif
