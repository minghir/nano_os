#ifndef PCI_H
#include <stdint.h>

// Funcții de bază pentru citire/scriere în spațiul de configurare PCI
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

// Funcția principală de scanare a magistralelor
void pcie_init(); // sau pci_scan_all()
void pci_scan_bus();

#endif