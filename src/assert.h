#ifndef _ASSERT_H
#define _ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) \
    ((expression) ? (void)0 : __assert_fail(#expression, __FILE__, __LINE__))

static inline void __assert_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", expr, file, line);
    abort(); // sau exit(1)
}
#endif

#endif