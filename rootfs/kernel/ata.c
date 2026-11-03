#include "io.h"
#include "ata.h"
#include "syslog.h"

// Declarăm funcția de print pentru a putea semnala erorile de timeout pe ecran
extern void print(const char* s);
extern void newline();

#define ATA_TIMEOUT 1000000 // Numărul maxim de încercări înainte de a renunța

/**
 * Funcție generalizată pentru citire, suportă drive:
 * 0 = Master (hda - NAN2)
 * 1 = Slave  (hdb - NanoFS V3)
 */
void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    uint32_t timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        KLOG_ERROR("[ATA ERROR] Read timeout: Disk is busy (BSY)\n");
        return;
    }

    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    
    // Selectăm drive-ul (0xE0 pentru Master, 0xF0 pentru Slave)
    uint8_t drive_select = (drive == 0) ? 0xE0 : 0xF0;
    outb(0x1F6, drive_select | ((lba >> 24) & 0x0F)); 

    // ---- ATA 400ns DELAY NECESAR LA COMUTAREA MASTER/SLAVE ----
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    // -----------------------------------------------------------

    outb(0x1F7, 0x20); // Comandă citire

    timeout = ATA_TIMEOUT;
    while (!(inb(0x1F7) & 0x08) && timeout > 0) {
        if (inb(0x1F7) & 0x01) {
            KLOG_ERROR("[ATA ERROR] Read failed: Controller error bit set (ERR)\n");
            return;
        }
        timeout--;
    }
    if (timeout == 0) {
        KLOG_ERROR("[ATA ERROR] Read timeout: Data request (DRQ) never arrived\n");
        return;
    }

    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0);
    }
}

/**
 * Funcție generalizată pentru scriere, suportă drive:
 * 0 = Master (hda - NAN2)
 * 1 = Slave  (hdb - NanoFS V3)
 */
void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer) {
    uint32_t timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        KLOG_ERROR("[ATA ERROR] Write timeout: Disk is busy (BSY)\n");
        return;
    }

    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    
    uint8_t drive_select = (drive == 0) ? 0xE0 : 0xF0;
    outb(0x1F6, drive_select | ((lba >> 24) & 0x0F)); 

    // ---- ATA 400ns DELAY NECESAR LA COMUTAREA MASTER/SLAVE ----
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    // -----------------------------------------------------------

    outb(0x1F7, 0x30); // Comandă scriere

    timeout = ATA_TIMEOUT;
    while (!(inb(0x1F7) & 0x08) && timeout > 0) {
        if (inb(0x1F7) & 0x01) {
            KLOG_ERROR("[ATA ERROR] Write failed: Controller error bit set (ERR)\n");
            return;
        }
        timeout--;
    }
    if (timeout == 0) {
        KLOG_ERROR("[ATA ERROR] Write timeout: Controller never requested data (DRQ)\n");
        return;
    }

    const uint16_t* ptr = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    outb(0x1F7, 0xE7); // Flush
    timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
}

// Wrapper-e pentru compatibilitate înapoi cu codul existent (folosesc implicit hda / drive 0)
void disk_read_sector(uint32_t lba, uint8_t* buffer) {
    disk_read_sector_drive(0, lba, buffer);
}

void disk_write_sector(uint32_t lba, const uint8_t* buffer) {
    disk_write_sector_drive(0, lba, buffer);
}