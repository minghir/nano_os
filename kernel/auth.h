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

// O tabelă internă de utilizatori în kernel
static UserAccount user_database[MAX_USERS] = {
    { "root", 0, 0, "/root" },
    { "user", 1000, 1000, "/home/user" }
};

uint32_t get_uid();

#endif