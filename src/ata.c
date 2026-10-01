/*
#include "io.h"
#include "ata.h"

void disk_read_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Așteptăm ca discul să nu fie ocupat (bitul 7 - BSY să fie 0)
    while (inb(0x1F7) & 0x80);

    // 2. Setăm parametrii de citire
    outb(0x1F2, 1);                  // Numărul de sectoare (1 sector = 512 octeți)
    outb(0x1F3, (uint8_t)(lba & 0xFF));             // LBA Low
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));      // LBA Mid
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));     // LBA High
    
    // Selectăm modul LBA și drive-ul Master (0xE0 | biții 24-27 ai LBA-ului)
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); 

    // 3. Trimitem comanda de citire (Read Sectors)
    outb(0x1F7, 0x20);

    // 4. Așteptăm ca discul să fie pregătit să furnizeze date (bitul 3 - DRQ să fie 1)
    while (!(inb(0x1F7) & 0x08));

    // 5. Citim cei 512 octeți (câte 2 octeți odată, adică 256 de cuvinte de 16 biți)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0); // Funcție care citește un cuvânt (16 biți) de pe port
    }
}

void disk_write_sector(uint32_t lba, const uint8_t* buffer) {
    // 1. Așteptăm ca discul să nu fie ocupat (BSY = 0)
    while (inb(0x1F7) & 0x80);

    // 2. Setăm parametrii pentru scriere
    outb(0x1F2, 1);                              // Numărul de sectoare (1 sector)
    outb(0x1F3, (uint8_t)(lba & 0xFF));          // LBA Low
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));   // LBA Mid
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));  // LBA High
    
    // Selectăm modul LBA și master/slave (0xE0 | biții 24-27)
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); 

    // 3. Trimitem comanda de scriere (Write Sectors: 0x30)
    outb(0x1F7, 0x30);

    // 4. Așteptăm ca discul să fie pregătit să primească date (DRQ = 1)
    while (!(inb(0x1F7) & 0x08));

    // 5. Trimitem cei 512 octeți (256 de cuvinte de 16 biți)
    const uint16_t* ptr = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    // 6. Trimitem comanda de Flush (opțional, dar recomandat pentru a asigura scrierea pe disc)
    outb(0x1F7, 0xE7);
    while (inb(0x1F7) & 0x80);
}
*/

#include "io.h"
#include "ata.h"

// Declarăm funcția de print pentru a putea semnala erorile de timeout pe ecran
extern void print(const char* s);
extern void newline();

#define ATA_TIMEOUT 1000000 // Numărul maxim de încercări înainte de a renunța

void disk_read_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Așteptăm ca discul să nu fie ocupat (bitul 7 - BSY să fie 0) cu timeout
    uint32_t timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        print("[ATA ERROR] Read timeout: Disk is busy (BSY)\n");
        return;
    }

    // 2. Setăm parametrii de citire
    outb(0x1F2, 1);                     // Numărul de sectoare (1 sector = 512 octeți)
    outb(0x1F3, (uint8_t)(lba & 0xFF));             // LBA Low
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));      // LBA Mid
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));     // LBA High
    
    // Selectăm modul LBA și drive-ul Master (0xE0 | biții 24-27 ai LBA-ului)
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); 

    // 3. Trimitem comanda de citire (Read Sectors)
    outb(0x1F7, 0x20);

    // 4. Așteptăm ca discul să fie pregătit să furnizeze date (bitul 3 - DRQ să fie 1) cu timeout
    timeout = ATA_TIMEOUT;
    while (!(inb(0x1F7) & 0x08) && timeout > 0) {
        // Verificăm dacă a apărut o eroare hardware (bitul 0 - ERR)
        if (inb(0x1F7) & 0x01) {
            print("[ATA ERROR] Read failed: Controller error bit set (ERR)\n");
            return;
        }
        timeout--;
    }
    if (timeout == 0) {
        print("[ATA ERROR] Read timeout: Data request (DRQ) never arrived\n");
        return;
    }

    // 5. Citim cei 512 octeți (câte 2 octeți odată, adică 256 de cuvinte de 16 biți)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(0x1F0); // Funcție care citește un cuvânt (16 biți) de pe port
    }
}

void disk_write_sector(uint32_t lba, const uint8_t* buffer) {
    // 1. Așteptăm ca discul să nu fie ocupat (BSY = 0) cu timeout
    uint32_t timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        print("[ATA ERROR] Write timeout: Disk is busy (BSY)\n");
        return;
    }

    // 2. Setăm parametrii pentru scriere
    outb(0x1F2, 1);                               // Numărul de sectoare (1 sector)
    outb(0x1F3, (uint8_t)(lba & 0xFF));          // LBA Low
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));   // LBA Mid
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));  // LBA High
    
    // Selectăm modul LBA și master/slave (0xE0 | biții 24-27)
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); 

    // 3. Trimitem comanda de scriere (Write Sectors: 0x30)
    outb(0x1F7, 0x30);

    // 4. Așteptăm ca discul să fie pregătit să primească date (DRQ = 1) cu timeout
    timeout = ATA_TIMEOUT;
    while (!(inb(0x1F7) & 0x08) && timeout > 0) {
        if (inb(0x1F7) & 0x01) {
            print("[ATA ERROR] Write failed: Controller error bit set (ERR)\n");
            return;
        }
        timeout--;
    }
    if (timeout == 0) {
        print("[ATA ERROR] Write timeout: Controller never requested data (DRQ)\n");
        return;
    }

    // 5. Trimitem cei 512 octeți (256 de cuvinte de 16 biți)
    const uint16_t* ptr = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }

    // 6. Trimitem comanda de Flush pentru a asigura scrierea pe disc (cu timeout)
    outb(0x1F7, 0xE7);
    timeout = ATA_TIMEOUT;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
}