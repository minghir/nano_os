#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

// Structura vtable pentru orice placă de rețea (NIC)
typedef struct {
    const char* name;
    void (*init)(uint64_t mmio_base, uint8_t irq);
    void (*send_packet)(const uint8_t* data, uint32_t length);
    void (*get_mac)(uint8_t* mac_out);
} NetDriver;

// Funcții publice globale ale subsistemului de rețea
void net_register_driver(NetDriver* driver);
void net_send_packet(const uint8_t* data, uint32_t length);
void net_get_mac(uint8_t* mac_out);


void test_mac();
void net_send_test_packet(void);

void net_send_ping(uint32_t dest_ip);

#endif