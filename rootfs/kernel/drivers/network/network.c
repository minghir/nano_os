#include "network.h"
#include "../../syslog.h" // pentru print-uri de kernel
#include "../../string.h" // pentru print-uri de kernel

#include "../../../thirdparty/uip/uip/uip.h"
#include "../../../thirdparty/uip/uip/uip_arp.h"

// Bufferul global pe care uIP îl folosește pentru I/O (trebuie să aibă exact dimensiunea UIP_CONF_BUFFER_SIZE)
// Folosim dimensiunea exactă pe care o așteaptă uIP (UIP_BUFSIZE + 2 = 1502 octeți)
//uint8_t uip_buf[1502] __attribute__((aligned(16)));

extern void e1000_receive_packets(void);
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
    
    for (uint32_t i = 0; i < msg_len && (14 + i) < sizeof(packet); i++) {
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


// Funcții helper pentru blocarea întreruperilor pe x86-64
static inline uint64_t net_lock(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags));
    return flags;
}

static inline void net_unlock(uint64_t flags) {
    __asm__ volatile ("pushq %0; popfq" : : "r"(flags));
}

void net_enqueue_packet(const uint8_t* data, uint16_t length) {
    if (length >= 14) {
        uint8_t my_mac[6];
        net_get_mac(my_mac);

        int sent_by_us = 1;
        for (int i = 0; i < 6; i++) {
            if (data[6 + i] != my_mac[i]) {
                sent_by_us = 0;
                break;
            }
        }
        if (sent_by_us) return;
    }

    // Punem ORICE pachet valid (IP sau ARP) în coada protejată
    uint64_t flags = net_lock();
    int next = (rx_head + 1) % MAX_RX_QUEUE;
    if (next != rx_head) {
        for (int i = 0; i < length; i++) {
            rx_queue[rx_head].data[i] = data[i];
        }
        rx_queue[rx_head].length = length;
        rx_queue[rx_head].ready = 1;
        rx_head = next;
    }
    net_unlock(flags);
}

int net_dequeue_packet(uint8_t* buffer, uint16_t max_len) {
    // Protejăm secțiunea critică a cozii
    uint64_t flags = net_lock();
    if (rx_tail == rx_head && !rx_queue[rx_tail].ready) {
        net_unlock(flags);
        return 0; 
    }
    int len = rx_queue[rx_tail].length;
    if (len > max_len) len = max_len;

    for (int i = 0; i < len; i++) {
        buffer[i] = rx_queue[rx_tail].data[i];
    }
    rx_queue[rx_tail].ready = 0;
    rx_tail = (rx_tail + 1) % MAX_RX_QUEUE;
    net_unlock(flags);
    return len;
}



void nano_start_tcp_server(void) {
	return;
    // Ascultă pe portul 8080 (convertit în network byte order cu HTONS)
    uip_listen(htons(8080));
	uip_listen(htons(23));
}

/*
void nano_uip_appcall(void) {
    if (uip_connected()) {
        KLOG_INFO("[TCP] Conexiune stabilita cu succes!\n");
    }
    
    if (uip_newdata()) {
        KLOG_INFO("[TCP] Date primite de la client!\n");
    }

    // Dacă clientul cere date sau s-a conectat, îi trimitem un răspuns HTTP simplu
    if (uip_connected() || uip_newdata()) {
        const char* html = "HTTP/1.0 200 OK\r\nContent-Length: 23\r\n\r\nHello from Nano OS TCP!";
        uip_send(html, 48);
    }
}
*/


// Asigură-te că uIP este configurat să folosească o structură de stare în appstate (dacă e suportat de uIP.conf)
// Sau putem folosi variabile statice simple pentru o sesiune unică de test.

extern void socket_uip_appcall(void);

void nano_uip_appcall(void) {
    uint16_t lport = ntohs(uip_conn->lport);

    if (lport == 8080) {
        // Păstrăm serverul HTTP static original pe portul 8080
        if (uip_connected()) {
            const char* response = "HTTP/1.0 200 OK\r\nContent-Length: 23\r\n\r\nHello from Nano OS TCP!";
            uip_send(response, string_length(response));
        }
        if (uip_acked()) {
            uip_close();
        }
    } else {
        // Toate celelalte porturi (cum ar fi 2323 pentru telnetd din User-Space) 
        // sunt preluate automat de subsistemul nostru de socket-uri!
        socket_uip_appcall();
    }
}



// Setează adresele IP inițiale pentru uIP (de ex: Nano OS = 10.0.2.15, Gateway = 10.0.2.2)
void nano_uip_init_network(void) {
    uip_ipaddr_t ipaddr;

    // Setează IP-ul nostru: 10.0.2.15
    uip_ipaddr(ipaddr, 10, 0, 2, 15);
    uip_sethostaddr(ipaddr);

    // Setează Gateway-ul: 10.0.2.2
    uip_ipaddr(ipaddr, 10, 0, 2, 2);
    uip_setdraddr(ipaddr);

    // Setează Masca de rețea: 255.255.255.0
    uip_ipaddr(ipaddr, 255, 255, 255, 0);
    uip_setnetmask(ipaddr);
}

// Funcția cheie apelată când sosește un pachet fizic de la e1000
void nano_uip_feed_packet(const uint8_t* data, uint16_t length) {
    if (length > sizeof(uip_buf)) return;

    for (uint16_t i = 0; i < length; i++) {
        uip_buf[i] = data[i];
    }
    uip_len = length;

    uint16_t eth_type = (uip_buf[12] << 8) | uip_buf[13];

    if (eth_type == 0x0806) {
        // Pachet ARP: lăsăm uIP să proceseze (învață MAC-ul gateway-ului / răspunde)
        uip_arp_arpin();
        if (uip_len > 0) {
            net_send_raw_packet(uip_buf, uip_len);
        }
    } 
    else if (eth_type == 0x0800) {
        // Actualizăm tabela ARP din headerul IP și trimitem în uIP
        uip_arp_ipin();
        uip_input();

        // Dacă uIP a generat un răspuns (ex: SYN-ACK), îl trimitem pe rețea
        if (uip_len > 0) {
            uip_arp_out(); 
            net_send_raw_packet(uip_buf, uip_len);
        }
    }
}


// Funcție periodică apelată din timer pentru a gestiona timerele TCP și retransmisiile
// Funcție periodică apelată din timer (la fiecare 0.5 secunde)
extern uint64_t kernel_cr3;

void net_poll_periodic(void) {
    uint64_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));

    if (kernel_cr3 != 0 && old_cr3 != kernel_cr3) {
        __asm__ volatile("mov %0, %%cr3" :: "r"(kernel_cr3));
    }

    // --- POLLED RX: Verificăm direct placa e1000 dacă a sosit ceva ---
    e1000_receive_packets();

    // Scoatem din coadă și trimitem la uIP
    uint8_t temp_packet[1500];
    int len = net_dequeue_packet(temp_packet, sizeof(temp_packet));
    if (len > 0) {
        nano_uip_feed_packet(temp_packet, len);
    }

    for (int i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);
        if (uip_len > 0) {
            uip_arp_out();
            net_send_raw_packet(uip_buf, uip_len);
        }
    }

    if (kernel_cr3 != 0 && old_cr3 != kernel_cr3) {
        __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));
    }
}

void net_subsystem_init(void) {
    uip_init();              // 1. Resetează și pregătește stiva uIP
    nano_uip_init_network(); // 2. Setează IP-ul, Gateway-ul și masca
    nano_start_tcp_server(); // 3. Pornește ascultătorul pe portul 8080
}