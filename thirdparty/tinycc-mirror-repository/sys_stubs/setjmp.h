#ifndef SETJMP_H
#define SETJMP_H
typedef long jmp_buf[8];
static inline int setjmp(jmp_buf env) { (void)env; return 0; }
static inline void longjmp(jmp_buf env, int val) { (void)env; (void)val; }
#endif
