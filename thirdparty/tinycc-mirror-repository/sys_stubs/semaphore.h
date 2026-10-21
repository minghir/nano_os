#ifndef SYS_STUBS_SEMAPHORE_H
#define SYS_STUBS_SEMAPHORE_H

typedef int sem_t;
static inline int sem_init(sem_t *sem, int pshared, unsigned int value) { (void)sem; (void)pshared; (void)value; return 0; }
static inline int sem_wait(sem_t *sem) { (void)sem; return 0; }
static inline int sem_post(sem_t *sem) { (void)sem; return 0; }
static inline int sem_destroy(sem_t *sem) { (void)sem; return 0; }

#endif
