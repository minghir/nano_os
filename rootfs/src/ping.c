#include "nano_libc.h"


// Funcție de concatenare a stringurilor pentru afișare
static void append_str(char* dest, const char* src, int* offset) {
    int i = 0;
    while (src[i] != '\0') {
        dest[(*offset)++] = src[i++];
    }
    dest[*offset] = '\0';
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    nano_print("PING 10.0.2.2 56 bytes of data.\n");
	uint32_t target_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2;
	// Schimbăm IP-ul țintă în 8.8.8.8
	//uint32_t target_ip = (8 << 24) | (8 << 16) | (8 << 8) | 8;
    

    int res = nano_ping(target_ip);
    if (res < 0) {
        nano_print("Erore la trimiterea pachetului ICMP.\n");
        return 1;
    }

    uint8_t rx_buffer[1500];
    int received = 0;
    
    for (int attempts = 0; attempts < 30; attempts++) {
        int bytes = nano_net_recv(rx_buffer, sizeof(rx_buffer));
        
        if (bytes > 0) {
            // Extragem EtherType
            uint16_t eth_type = (rx_buffer[12] << 8) | rx_buffer[13];
            
            // --- DIAGNOSTIC START ---
            char buf[16];
            nano_print("[DEBUG] Pachet prins: ");
            simple_itoa(bytes, buf); nano_print(buf);
            nano_print(" octeti | EtherType: ");
            simple_itoa(eth_type, buf); nano_print(buf); // 2048 = IPv4, 2054 = ARP
            
            if (eth_type == 0x0800) { // Dacă e IPv4
                uint8_t protocol = rx_buffer[23];
                nano_print(" | Protocol IP: ");
                simple_itoa(protocol, buf); nano_print(buf); // 1 = ICMP
                
                if (protocol == 1) { // Dacă e ICMP
                    uint8_t ip_ihl = rx_buffer[14] & 0x0F;
                    uint32_t icmp_offset = 14 + (ip_ihl * 4);
                    uint8_t icmp_type = rx_buffer[icmp_offset];
                    
                    nano_print(" | ICMP Type: ");
                    simple_itoa(icmp_type, buf); nano_print(buf); // 0 = Reply, 8 = Request
                }
            }
            nano_print("\n");
            // --- DIAGNOSTIC END ---

            // Parsarea strictă (codul original)
            if (bytes >= 42 && eth_type == 0x0800) {
                uint8_t ip_ihl = rx_buffer[14] & 0x0F;
                uint32_t ip_header_len = ip_ihl * 4;
                uint8_t protocol = rx_buffer[23];

                if (protocol == 1) {
                    uint32_t icmp_offset = 14 + ip_header_len;
                    uint8_t icmp_type = rx_buffer[icmp_offset];
                    
                    if (icmp_type == 0) {
                        nano_print("64 bytes from 10.0.2.2: (Răspuns valid parsat cu succes!)\n");
                        received = 1;
                        break;
                    }
                }
            }
        }
        
        nano_sleep(50);
    }

    if (!received) {
        nano_print("Request timed out (niciun pachet primit).\n");
    }

    return 0;
}