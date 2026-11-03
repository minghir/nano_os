#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include "../network.h" // Presupunând că ai definit NetDriver în net.h
#include "../../../pcb.h"

// Descriptorii e1000 (echivalentul lui ac97_bd_t de la sunet)
typedef struct {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc;

typedef struct {
    uint64_t address;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc;

// Funcția publică care expune vtable-ul către subsistemul PCI / Net
NetDriver* e1000_get_driver(void);

void e1000_receive_packets(void);
void e1000_irq_handler(Registers* regs);

#endif