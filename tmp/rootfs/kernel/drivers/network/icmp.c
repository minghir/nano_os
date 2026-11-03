#include "network.h"
#include "../../string.h"
#include "../../syslog.h"

// Funcție ajutătoare pentru calculul Checksum (IP / ICMP)
static uint16_t net_checksum(const uint8_t* data, int length) {
    uint32_t sum = 0;
    // Citim datele sub formă de cuvinte de 16 biți (Big-Endian)
    for (int i = 0; i < length - 1; i += 2) {
        sum += ((uint16_t)data[i] << 8) | data[i+1];
    }
    // Dacă lungimea este impară, adăugăm ultimul byte aliniat
    if (length % 2 != 0) {
        sum += ((uint16_t)data[length - 1] << 8);
    }
    // "Fold" la 16 biți (adunăm carry-ul)
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum); // Complement față de 1
}

void net_send_ping(uint32_t dest_ip) {
    uint8_t packet[128];
    // IMPORTANT: Curățăm bufferul cu 0 pentru a nu avea "gunoi" în padding-ul Ethernet!
    for(int i = 0; i < 128; i++) packet[i] = 0;
    
    // --- 1. HEADER ETHERNET (14 octeți) ---
    // Destinație MAC: Routerul QEMU SLIRP (10.0.2.2)
    packet[0] = 0x52; packet[1] = 0x55; packet[2] = 0x0A;
    packet[3] = 0x00; packet[4] = 0x02; packet[5] = 0x02;

    // Sursă MAC: Adresa noastră reală
    uint8_t my_mac[6];
    net_get_mac(my_mac);
    for (int i = 0; i < 6; i++) packet[6 + i] = my_mac[i];

    // EtherType: IPv4 (0x0800)
    packet[12] = 0x08; packet[13] = 0x00;

    // --- CALCUL LUNGIMI DINAMICE ---
    const char* icmp_data = "NanoOS-Ping-Data";
    int data_len = string_length(icmp_data); // 16 octeți
    int icmp_total_len = 8 + data_len;       // 8 + 16 = 24 octeți
    int ip_total_len = 20 + icmp_total_len;  // 20 + 24 = 44 octeți

    // --- 2. HEADER IPV4 (20 octeți) ---
    packet[14] = 0x45; // Version 4, IHL 5
    packet[15] = 0x00; // TOS
    
    // Total Length (Calculat dinamic! - Big Endian)
    packet[16] = (ip_total_len >> 8) & 0xFF;
    packet[17] = ip_total_len & 0xFF;
    
    packet[18] = 0xAB; packet[19] = 0xCD; // Identification
    packet[20] = 0x00; packet[21] = 0x00; // Flags & Fragment Offset
    packet[22] = 64;   // TTL
    packet[23] = 1;    // Protocol: ICMP (1)
    
    // Checksum IP (Zero înainte de calcul)
    packet[24] = 0x00; packet[25] = 0x00; 

    // IP Sursă: 10.0.2.15
    packet[26] = 10; packet[27] = 0; packet[28] = 2; packet[29] = 15;

    // IP Destinație: 10.0.2.2
    packet[30] = (dest_ip >> 24) & 0xFF;
    packet[31] = (dest_ip >> 16) & 0xFF;
    packet[32] = (dest_ip >> 8) & 0xFF;
    packet[33] = dest_ip & 0xFF;

    // Calcul Checksum IP
    uint16_t ip_chk = net_checksum(&packet[14], 20);
    packet[24] = (ip_chk >> 8) & 0xFF;
    packet[25] = ip_chk & 0xFF;

    // --- 3. HEADER ICMP (Echo Request) ---
    packet[34] = 8;    // Type: 8 (Echo Request)
    packet[35] = 0;    // Code: 0
    packet[36] = 0; packet[37] = 0; // Checksum ICMP (Zero înainte de calcul)
    packet[38] = 0x12; packet[39] = 0x34; // Identifier
    packet[40] = 0x00; packet[41] = 0x01; // Sequence Number

    // Payload ICMP
    for (int i = 0; i < data_len; i++) {
        packet[42 + i] = (uint8_t)icmp_data[i];
    }

    // Calcul Checksum ICMP
    uint16_t icmp_chk = net_checksum(&packet[34], icmp_total_len);
    packet[36] = (icmp_chk >> 8) & 0xFF;
    packet[37] = icmp_chk & 0xFF;

    // --- 4. TRIMITERE PACHET ---
    uint32_t total_frame_len = 14 + ip_total_len;

    // Padding obligatoriu Ethernet pentru Runt Frames (min 60 octeți)
    if (total_frame_len < 60) {
        total_frame_len = 60; 
    }

    KLOG_INFO("[NET] Trimitere ICMP Echo Request (Ping)...\n");
    net_send_packet(packet, total_frame_len);
    KLOG_INFO("[NET] Pachet Ping trimis!\n");
}