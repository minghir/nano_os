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


#define MAX_RX_QUEUE 16
typedef struct {
    uint8_t data[2048];
    uint16_t length;
    int ready;
} RxPacket;

static RxPacket rx_queue[MAX_RX_QUEUE];
static int rx_head = 0;
static int rx_tail = 0;

void net_enqueue_packet(const uint8_t* data, uint16_t length);
int net_dequeue_packet(uint8_t* buffer, uint16_t max_len);

void nano_uip_appcall(void);
void nano_uip_init_network(void);
void nano_uip_feed_packet(const uint8_t* data, uint16_t length);
void nano_start_tcp_server(void);
void net_poll_periodic(void);
void net_subsystem_init(void);

#endif