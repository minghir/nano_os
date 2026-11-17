#include "nano_libc.h"
#include "stdio.h"
#include "string.h"

// Funcție de potrivire cu wildcard ('*')
int match_pattern(const char* str, const char* pattern) {
    int s = 0, p = 0;
    int star = -1;
    int match = 0;

    while (str[s] != '\0') {
        if (pattern[p] == str[s]) {
            s++;
            p++;
        } else if (pattern[p] == '*') {
            star = p;
            match = s;
            p++;
        } else if (star != -1) {
            p = star + 1;
            match++;
            s = match;
        } else {
            return 0;
        }
    }

    while (pattern[p] == '*') {
        p++;
    }

    return pattern[p] == '\0';
}

int main(int argc, char* argv[]) {
    const char* arg = (argc > 1) ? argv[1] : ".";

    // 1. Separăm calea (directorul) de șablon (pattern) dacă există un '/'
    char dir_path[128] = ".";
    char pattern[128];
    strcpy(pattern, arg);

    char *last_slash = strrchr(arg, '/');
    if (last_slash) {
        int dir_len = last_slash - arg;
        if (dir_len == 0) {
            strcpy(dir_path, "/");
        } else {
            for (int i = 0; i < dir_len; i++) dir_path[i] = arg[i];
            dir_path[dir_len] = '\0';
        }
        strcpy(pattern, last_slash + 1);
    }

    // 2. Verificăm dacă șablonul conține wildcard ('*')
    int has_wildcard = 0;
    for (int i = 0; pattern[i] != '\0'; i++) {
        if (pattern[i] == '*') {
            has_wildcard = 1;
            break;
        }
    }

    if (has_wildcard) {
        // Salvăm directorul curent pentru a ne putea întoarce la final
        char old_cwd[64];
        nano_getcwd(old_cwd, sizeof(old_cwd));

        // Ne mutat temporar în directorul cerut
        if (last_slash) {
            nano_cd(dir_path);
        }

        char filename[64];
        int index = 0;
        int found = 0;

        // Iterăm prin fișierele din directorul țintă
        while (nano_get_file_name_at(index, filename, sizeof(filename))) {
            if (match_pattern(filename, pattern)) {
                printf("%s\n", filename);
                found = 1;
            }
            index++;
        }

        // Revenim în directorul de unde am plecat
        if (last_slash) {
            nano_cd(old_cwd);
        }
    } else {
        // Comportament normal pentru director sau cale fixă
        nano_ls(arg);
    }

    return 0;
}