#ifndef SYS_STUBS_STDDEF_H
#define SYS_STUBS_STDDEF_H
#include "stdint.h"
typedef unsigned long size_t;
typedef long ptrdiff_t;
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif
