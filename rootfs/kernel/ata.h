#ifndef ATA_H
#define ATA_H
#include <stdint.h>

void disk_read_sector(uint32_t lba, uint8_t* buffer);
void disk_write_sector(uint32_t lba, const uint8_t* buffer);

// Noile funcții multi-drive pentru hdb (drive 1)
void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

#endif