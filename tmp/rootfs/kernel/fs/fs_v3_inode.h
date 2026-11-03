#ifndef FS_V3_INODE_H
#define FS_V3_INODE_H

#include <stdint.h>
#include "fs_v3.h"
#include "../syslog.h"


/**
 * Citește un inod după indexul său global de pe disc.
 */
int nfs3_read_inode(uint32_t inode_index, InodeV3* out_inode);

/**
 * Scrie un inod pe disc la indexul specificat.
 */
int nfs3_write_inode(uint32_t inode_index, const InodeV3* inode);

/**
 * Caută primul inod neutilizat (UNUSED) și îl rezervă.
 * Returnează indexul inodului alocat sau 0 în caz de eșec (tabel de inoduri plin).
 */
uint32_t nfs3_alloc_inode(void);
/**
 * Eliberează un inod, marcându-l caUNUSED.
 */
void nfs3_free_inode(uint32_t inode_index);

#endif