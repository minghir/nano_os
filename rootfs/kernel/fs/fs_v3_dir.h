#ifndef FS_V3_DIR_H
#define FS_V3_DIR_H

#include <stdint.h>
#include "fs_v3.h"

// --- API-ul pentru Directoare și Fișiere V3 ---

// Inițializează sau verifică structura de directoare pe hdb
void nfs3_dir_init(void);

// Listează conținutul unui director pe baza indexului de inod
void nfs3_list_dir(uint32_t dir_inode_index);

// Caută o intrare după nume în interiorul unui director; returnează indexul inodului sau 0 dacă nu există
uint32_t nfs3_find_in_dir(uint32_t dir_inode_index, const char* name);

// Creează un fișier sau un director nou în directorul părinte specificat
int nfs3_create_entry(uint32_t parent_inode_index, const char* name, uint8_t flags, uint32_t initial_size);

// Șterge un fișier sau director din părinte
int nfs3_delete_entry(uint32_t parent_inode_index, const char* name);

#endif