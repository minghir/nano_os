//Acesta conține logica de interogare prin porturile I/O 0xCF8 și 0xCFC și bucla care parcurge magistralele, sloturile și funcțiile.

#include "pci.h"
#include "io.h"     
#include "syslog.h"
#include "string.h"
#include "drivers/sound/audio.h"
#include "drivers/sound/ac97/ac97.h"
#include "drivers/network/network.h"
#include "drivers/network/e1000/e1000.h"
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
					KLOG_INFO("[PCI] Gasit controler audio Intel AC'97!\n");
					
					// 1. Citim BAR0 (Mixer Base Address) și BAR1 (Bus Master Base Address)
					uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
					uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
					
					// Curățăm biții de flag (ultimii 2 biți indică tipul de adresă I/O sau Memorie)
					uint16_t mixer_base = (uint16_t)(bar0 & 0xFFFC);
					uint16_t bm_base = (uint16_t)(bar1 & 0xFFFC);

					// 2. Activăm Bus Master în Command Register (Offset 0x04)
					uint32_t cmd_reg = pci_config_read(bus, slot, func, 0x04);
					cmd_reg |= (1 << 2); // Setăm bitul 2 (Bus Master Enable)
					pci_config_write(bus, slot, func, 0x04, cmd_reg);

					// 3. Înregistrăm driverul în subsistemul audio general
					audio_register_driver(ac97_get_driver());

					// 4. Inițializăm driverul folosind porturile REALE extrase din PCI!
					AudioDriver* audio_drv = ac97_get_driver();
					if (audio_drv && audio_drv->init) {
						audio_drv->init(mixer_base, bm_base);
					}
				}
				
				// Verificăm dacă este Intel e1000 Network Card (Vendor: 0x8086, Device: 0x100E)
				if (vendor_id == 0x8086 && device_id == 0x100E) {
					KLOG_INFO("[PCI] Gasit placa de retea Intel e1000!\n");
					
					// 1. E1000 folosește în principal BAR0 ca Memory-Mapped I/O (MMIO), nu porturi I/O!
					uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
					
					// Verificăm dacă BAR0 este de tip Memory Space (bitul 0 ar trebui să fie 0)
					uint64_t mmio_base = (bar0 & 0xFFFFFFF0); // Adresa fizică de 32 de biți

					// 2. Activăm Bus Master (bit 2) și Memory Space Access (bit 1) în Command Register (Offset 0x04)
					uint32_t cmd_reg = pci_config_read(bus, slot, func, 0x04);
					cmd_reg |= (1 << 2) | (1 << 1); // Bus Master + Memory Space
					cmd_reg &= ~(1 << 10);          // (Șterge bitul "Interrupt Disable")
					pci_config_write(bus, slot, func, 0x04, cmd_reg);

					// 3. Citim linia de întrerupere (IRQ) de la offset-ul 0x3C (байtul inferior)
					uint32_t reg_line = pci_config_read(bus, slot, func, 0x3C);
					uint8_t irq = (uint8_t)(reg_line & 0xFF);
					
					// Folosim snprintf (pe care îl ai în string.h) ca să formatăm informația într-un buffer
					char log_buf[128];
					snprintf(log_buf, sizeof(log_buf), "   -> MMIO Base: 0x%x, IRQ Line: %d\n", (uint32_t)mmio_base, irq);
					KLOG_INFO(log_buf);

					// 4. Inițializăm driverul e1000 trecându-i adresa MMIO
					// Înregistrăm driverul în subsistemul de rețea
					net_register_driver(e1000_get_driver());

					// Inițializăm folosind interfețele vtable
					NetDriver* net_drv = e1000_get_driver();
					if (net_drv && net_drv->init) {
						net_drv->init(mmio_base, irq);
					}
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