#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

// Tipuri standard POSIX pentru timp
typedef long time_t;
typedef long clock_t;

// Structura standard tm pentru calendar / dată și oră
struct tm {
    int tm_sec;   // Secunde (0-60)
    int tm_min;   // Minute (0-59)
    int tm_hour;  // Ore (0-23)
    int tm_mday;  // Ziua lunii (1-31)
    int tm_mon;   // Luna (0-11)
    int tm_year;  // Anul minus 1900
    int tm_wday;  // Ziua săptămânii (0-6, duminică = 0)
    int tm_yday;  // Ziua din an (0-365)
    int tm_isdst; // Flag pentru ora de vară
};

// Constante de timp
#define CLOCKS_PER_SEC 1000

// Prototipuri de funcții POSIX / C standard
time_t time(time_t *tloc);
clock_t clock(void);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned long usec);

// Conversii opționale de bază (utile pentru log-uri sau afișaj)
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *localtime(const time_t *timer);
struct tm *gmtime(const time_t *timer);

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
time_t mktime(struct tm *timeptr);
double difftime(time_t time1, time_t time0);

#endif // _TIME_H