#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>

#define MAX_USERS 16
#define MAX_USERNAME_LEN 32

typedef struct {
    char username[MAX_USERNAME_LEN];
    uint32_t uid;
    uint32_t gid;
    char home_dir[64];
} UserAccount;

extern UserAccount user_database[MAX_USERS];

uint32_t get_uid(void);

#endif