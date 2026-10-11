#include "network.h"
#include "../../syslog.h" // pentru print-uri de kernel
#include "../../string.h" // pentru print-uri de kernel

static NetDriver* current_net_driver = 0;

void net_register_driver(NetDriver* driver) {
    current_net_driver = driver;
    KLOG_INFO("[NET] Inregistrat driver retea: ");
    if (driver && driver->name) {
        KLOG_INFO((char*)driver->name);
    }
    KLOG_INFO("\n");
}

void net_send_packet(const uint8_t* data, uint32_t length) {
    if (current_net_driver && current_net_driver->send_packet) {
        current_net_driver->send_packet(data, length);
    }
}

void net_get_mac(uint8_t* mac_out) {
    if (current_net_driver && current_net_driver->get_mac) {
        current_net_driver->get_mac(mac_out);
    }
}

void test_mac(){
	uint8_t mac[6];
	net_get_mac(mac);
	char mac_buf[64];
	snprintf(mac_buf, sizeof(mac_buf), "[NET] MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	KLOG_INFO(mac_buf);
}

void net_send_test_packet(void) {
    uint8_t packet[128];
    
    // 1. Destinație MAC: Broadcast (FF:FF:FF:FF:FF:FF)
    for (int i = 0; i < 6; i++) {
        packet[i] = 0xFF;
    }

    // 2. Sursă MAC: Luăm adresa noastră reală de la placă
    uint8_t my_mac[6];
    net_get_mac(my_mac);
    for (int i = 0; i < 6; i++) {
        packet[6 + i] = my_mac[i];
    }

    // 3. EtherType: Protocol de test personalizat (0x9000)
    packet[12] = 0x90;
    packet[13] = 0x00;

    // 4. Payload: Mesajul nostru
    const char* msg = "Hello from Nano OS Networking Stack!";
    int msg_len = string_length(msg);
    
    for (int i = 0; i < msg_len && (14 + i) < sizeof(packet); i++) {
        packet[14 + i] = (uint8_t)msg[i];
    }

    uint32_t total_len = 14 + msg_len;

    // Asigurăm dimensiunea minimă Ethernet (64 octeți cu tot cu CRC, sau cel puțin 60 fără CRC)
    if (total_len < 60) {
        total_len = 60;
    }

    KLOG_INFO("[NET] Trimitere pachet Ethernet de test...\n");
    
    // Trimitem prin interfața vtable
    net_send_packet(packet, total_len);
    
    KLOG_INFO("[NET] Pachet de test trimis cu succes prin e1000!\n");
}