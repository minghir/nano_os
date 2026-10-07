#include "nano_libc.h"

int main() {
    nano_syslog("[INIT] Nano OS Init daemon started (PID 1).\n");

    while (1) {
        nano_syslog("[INIT] Spawning shell (/sbin/shell)...\n");
        
        // Lansăm shell-ul
        int success = nano_exec("/sbin/shell");
        
        if (success) {
            // Așteptăm ca shell-ul să iasă (când utilizatorul dă 'exit')
            nano_wait();
            nano_print("[INIT] Shell terminated. Restarting in 2 seconds...\n");
        } else {
            nano_print("[INIT] Error: Failed to start /sbin/shell!\n");
        }
        
        // O scurtă pauză înainte de respawn pentru a evita o buclă de erori infinită
        sleep(2000);
    }

    return 0;
}