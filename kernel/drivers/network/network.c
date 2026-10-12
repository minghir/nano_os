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


// Funcție internă pură pentru a trimite un pachet direct prin driverul curent fără KLOG-uri
static void net_send_raw_packet(const uint8_t* data, uint32_t length) {
    if (current_net_driver && current_net_driver->send_packet) {
        current_net_driver->send_packet(data, length);
    }
}


// Asigură-te că ai extern pentru kernel_cr3 în network.c
extern uint64_t kernel_cr3;

static void net_handle_arp(const uint8_t* packet) {
    if ((packet[12] << 8 | packet[13]) != 0x0806) return;

    uint16_t opcode = (packet[20] << 8) | packet[21];
    if (opcode != 1) return;

    // Verificăm dacă ne caută pe noi (10.0.2.15)
    if (packet[38] == 10 && packet[39] == 0 && packet[40] == 2 && packet[41] == 15) {
        
        // STATIC BUFFER: Evită orice Stack Overflow pe stiva de întreruperi!
        static uint8_t reply[60];
        for (int i = 0; i < 60; i++) reply[i] = 0;

        uint8_t my_mac[6];
        net_get_mac(my_mac);

        // --- 1. HEADER ETHERNET ---
        for (int i = 0; i < 6; i++) reply[i] = packet[6 + i];       // Dest MAC (QEMU)
        for (int i = 0; i < 6; i++) reply[6 + i] = my_mac[i];      // Src MAC (Noi)
        reply[12] = 0x08; reply[13] = 0x06;                        // EtherType: ARP (0x0806)

        // --- 2. HEADER ARP ---
        reply[14] = 0x00; reply[15] = 0x01; // Hardware Type: Ethernet (1)
        reply[16] = 0x08; reply[17] = 0x00; // Protocol Type: IPv4 (0x0800)
        reply[18] = 6;                      // Hardware Size: 6
        reply[19] = 4;                      // Protocol Size: 4
        reply[20] = 0x00; reply[21] = 0x02; // Opcode: ARP Reply (2)

        // Sender MAC & IP (Noi)
        for (int i = 0; i < 6; i++) reply[22 + i] = my_mac[i];
        reply[28] = 10; reply[29] = 0; reply[30] = 2; reply[31] = 15;

        // Target MAC & IP (QEMU)
        for (int i = 0; i < 6; i++) reply[32 + i] = packet[22 + i];
        for (int i = 0; i < 4; i++) reply[38 + i] = packet[28 + i];

        // Trimitem răspunsul ARP înapoi pe rețea
        net_send_raw_packet(reply, 60);
    }
}


void net_enqueue_packet(const uint8_t* data, uint16_t length) {
    if (length >= 14) {
        // Obținem adresa noastră MAC
        uint8_t my_mac[6];
        net_get_mac(my_mac);

        // Verificăm dacă pachetul este trimis de NOI înșine (Sursa MAC din pachet == MAC-ul nostru)
        // Octeții 6-11 din headerul Ethernet reprezintă Sursa MAC
        int sent_by_us = 1;
        for (int i = 0; i < 6; i++) {
            if (data[6 + i] != my_mac[i]) {
                sent_by_us = 0;
                break;
            }
        }

        // Dacă pachetul este generat de propria noastră placă, îl ignorăm complet (evităm bucla infinită!)
        if (sent_by_us) {
            return;
        }

        uint16_t eth_type = (data[12] << 8) | data[13];
        
        // Dacă este un ARP Request extern, îi răspundem
        if (eth_type == 0x0806) {
            net_handle_arp(data);
            return; 
        }
    }

    // Coada normală pentru IP / ICMP / UDP externe
    int next = (rx_head + 1) % MAX_RX_QUEUE;
    if (next != rx_tail) {
        for (int i = 0; i < length; i++) {
            rx_queue[rx_head].data[i] = data[i];
        }
        rx_queue[rx_head].length = length;
        rx_queue[rx_head].ready = 1;
        rx_head = next;
    }
}

int net_dequeue_packet(uint8_t* buffer, uint16_t max_len) {
    if (rx_tail == rx_head && !rx_queue[rx_tail].ready) {
        return 0; 
    }
    int len = rx_queue[rx_tail].length;
    if (len > max_len) len = max_len;

    for (int i = 0; i < len; i++) {
        buffer[i] = rx_queue[rx_tail].data[i];
    }
    rx_queue[rx_tail].ready = 0;
    rx_tail = (rx_tail + 1) % MAX_RX_QUEUE;
    return len;
}