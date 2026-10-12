#ifndef __CLOCK_ARCH_H__
#define __CLOCK_ARCH_H__

// Tipul de date folosit de uIP pentru ceas (de obicei un enter pe 16 sau 32 de biți)
typedef unsigned long clock_time_t;

// Numărul de tick-uri de ceas pe secundă (de ex. 100 dacă ai un timer de 10ms sau 1000 dacă e la 1ms)
#define CLOCK_CONF_SECOND 100

#endif /* __CLOCK_ARCH_H__ */