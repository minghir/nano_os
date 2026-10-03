//Acesta conține logica de interogare prin porturile I/O 0xCF8 și 0xCFC și bucla care parcurge magistralele, sloturile și funcțiile.

#include "pci.h"
#include "io.h"     // Aici ai funcțiile outl, inl etc.
// #include "sys.h" // Dacă ai funcții de print/log în kernel

// Citirea unui registru PCI (pe 32 de biți)
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |               // Enable bit
        ((uint32_t)bus << 16) | 
        ((uint32_t)slot << 11) | 
        ((uint32_t)func << 8) | 
        (offset & 0xFC)           // Aliniat la 4 biți
    );
    
    outl(0xCF8, address);
    return inl(0xCFC);
}

// Scriere opțională în registrele PCI
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) | 
        ((uint32_t)bus << 16) | 
        ((uint32_t)slot << 11) | 
        ((uint32_t)func << 8) | 
        (offset & 0xFC)
    );
    
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// Funcție ajutătoare pentru a converti un ID în șir sau a-l afișa (opțional)
void pci_scan_bus() {
    print("Incepere scanare magistrala PCI...\n");

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                
                // Citim Vendor ID și Device ID de la offset-ul 0x00
                uint32_t reg0 = pci_config_read(bus, slot, func, 0x00);
                uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
                uint16_t device_id = (uint16_t)(reg0 >> 16);

                // Dacă Vendor ID este 0xFFFF, înseamnă că nu există niciun dispozitiv în acest slot
                if (vendor_id == 0xFFFF) {
                    // Dacă suntem la funcția 0 și e 0xFFFF, e posibil ca slotul să fie complet gol, 
                    // dar continuăm sau putem da break pe funcții pentru optimizare.
                    if (func == 0) break; 
                    continue;
                }

                // Am găsit un dispozitiv valid! Citim Header Type (offset 0x0C, bitul 7 ne spune dacă e multi-function)
                uint32_t reg3 = pci_config_read(bus, slot, func, 0x0C);
                uint8_t header_type = (uint8_t)((reg3 >> 16) & 0xFF);

                // Verificăm dacă este AC'97 Controller (Intel: Vendor 0x8086, Device 0x2415)
                if (vendor_id == 0x8086 && device_id == 0x2415) {
                    print("[PCI] Gasit controler audio Intel AC'97!\n");
                    
                    // Putem citi BAR0 și BAR1 (Base Address Registers) de la 0x10 și 0x14
                    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                    uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
                    
                    // Salvăm adresele sau le folosim mai târziu în driverul audio
                }

                // Dacă nu e multi-function și suntem la funcția 0, putem sărim peste restul funcțiilor din acest slot
                if (func == 0 && !(header_type & 0x80)) {
                    break;
                }
            }
        }
    }
    print("Scanare PCI finalizata.\n");
}