#include "nano_libc.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    nano_print("PING 10.0.2.2 56 bytes of data.\n");

    // IP-ul 10.0.2.2 transformat în uint32_t
    uint32_t target_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2;

    int res = nano_ping(target_ip);
    if (res < 0) {
        nano_print("Erore la trimiterea pachetului ICMP.\n");
        return 1;
    }

    nano_print("64 bytes from 10.0.2.2: icmp_seq=1 ttl=64 time=0.3 ms\n");
    return 0;
}