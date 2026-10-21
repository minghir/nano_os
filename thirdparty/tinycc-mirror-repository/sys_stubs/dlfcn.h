#ifndef DLFCN_H
#define DLFCN_H

#define RTLD_LAZY     1
#define RTLD_NOW      2
#define RTLD_GLOBAL   0x100
#define RTLD_LOCAL    0
#define RTLD_DEFAULT  ((void*)0)

static inline void* dlopen(const char* filename, int flags) { (void)filename; (void)flags; return (void*)0; }
static inline void* dlsym(void* handle, const char* symbol) { (void)handle; (void)symbol; return (void*)0; }
static inline int dlclose(void* handle) { (void)handle; return 0; }
static inline char* dlerror(void) { return "Dynamic loading not supported"; }

#endif
