#ifndef SYS_STUBS_TIME_H
#define SYS_STUBS_TIME_H

#include "stdint.h"

typedef int64_t time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

static inline time_t time(time_t *tloc) {
    if (tloc) *tloc = 0;
    return 0;
}

static inline struct tm *localtime(const time_t *timep) {
    static struct tm null_tm = {0, 0, 0, 1, 0, 120, 0, 0, 0};
    (void)timep;
    return &null_tm;
}

#endif
