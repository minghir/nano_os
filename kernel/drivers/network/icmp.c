#include "network.h"
#include "../../string.h"
#include "../../syslog.h"

// Funcție ajutătoare pentru calculul Checksum (IP / ICMP)
static uint16_t calculate_checksum(void* buffer, int length) {
    uint16_t* ptr = buffer;
    uint32_t sum = 0;
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    if (length > 0) {
        sum += *(uint8_t*)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

void net_send_ping(uint32_t dest_ip) {
    uint8_t packet[128];
    
    // --- 1. HEADER ETHERNET (14 octeți) ---
    // Destinație MAC: Broadcast sau MAC-ul gateway-ului (pentru simplificare în QEMU putem folosi broadcast FF:FF:FF:FF:FF:FF)
    for (int i = 0; i < 6; i++) packet[i] = 0xFF;

    // Sursă MAC: Adresa noastră
    uint8_t my_mac[6];
    net_get_mac(my_mac);
    for (int i = 0; i < 6; i++) packet[6 + i] = my_mac[i];

    // EtherType: IPv4 (0x0800)
    packet[12] = 0x08;
    packet[13] = 0x00;

    // --- 2. HEADER IPV4 (20 octeți) ---
    packet[14] = 0x45; // Version (4) + IHL (5)
    packet[15] = 0x00; // TOS
    packet[16] = 0x00; packet[17] = 54; // Total Length (20 IP + 8 ICMP + 26 data)
    packet[18] = 0xAB; packet[19] = 0xCD; // Identification
    packet[20] = 0x00; packet[21] = 0x00; // Flags & Fragment Offset
    packet[22] = 64;   // TTL (Time To Live)
    packet[23] = 1;    // Protocol: ICMP (1)
    packet[24] = 0x00; packet[25] = 0x00; // IP Checksum (calculat mai jos)

    // IP Sursă: 10.0.2.15 (IP-ul standard dat de QEMU pentru client)
    packet[26] = 10; packet[27] = 0; packet[28] = 2; packet[29] = 15;

    // IP Destinație (primit ca parametru, ex: 10.0.2.2 pentru gateway)
    packet[30] = (dest_ip >> 24) & 0xFF;
    packet[31] = (dest_ip >> 16) & 0xFF;
    packet[32] = (dest_ip >> 8) & 0xFF;
    packet[33] = dest_ip & 0xFF;

    // Calcul Checksum IP
    uint16_t ip_chk = calculate_checksum(&packet[14], 20);
    packet[24] = ip_chk & 0xFF;
    packet[25] = (ip_chk >> 8) & 0xFF;

    // --- 3. HEADER ICMP (Echo Request - 8 octeți + payload) ---
    packet[34] = 8;    // Type: 8 (Echo Request / Ping)
    packet[35] = 0;    // Code: 0
    packet[36] = 0; packet[37] = 0; // ICMP Checksum (completat după)
    packet[38] = 0x12; packet[39] = 0x34; // Identifier
    packet[40] = 0x00; packet[41] = 0x01; // Sequence Number

    // Payload ICMP ("NanoOS-Ping")
    const char* icmp_data = "NanoOS-Ping-Data";
    int data_len = string_length(icmp_data);
    for (int i = 0; i < data_len; i++) {
        packet[42 + i] = (uint8_t)icmp_data[i];
    }

    int icmp_total_len = 8 + data_len;
    uint16_t icmp_chk = calculate_checksum(&packet[34], icmp_total_len);
    packet[36] = icmp_chk & 0xFF;
    packet[37] = (icmp_chk >> 8) & 0xFF;

    uint32_t total_frame_len = 14 + 20 + icmp_total_len;

    KLOG_INFO("[NET] Trimitere ICMP Echo Request (Ping)...\n");
    net_send_packet(packet, total_frame_len);
    KLOG_INFO("[NET] Pachet Ping trimis!\n");
}