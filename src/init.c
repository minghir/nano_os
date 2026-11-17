#include "nano_libc.h"
#include "time.h"

int main() {
    nano_syslog("[INIT] Nano OS Init daemon started (PID 1).\n");

    while (1) {
        nano_syslog("[INIT] Spawning shell...\n");
        
        int pid = nano_exec("/hda/bin/shell");
        if (pid <= 0) pid = nano_exec("/hda/bin/shell");
        if (pid <= 0) pid = nano_exec("/bin/shell");

        if (pid > 0) {
            // Așteptăm ca shell-ul să se termine (SYSCALL_WAIT adoarme procesul init)
            nano_wait();
            nano_print("[INIT] Shell terminated. Restarting in 2 seconds...\n");
        } else {
            nano_print("[INIT] Error: Failed to start shell!\n");
        }
        
        // Pauză înainte de respawn
        sleep(2);
    }

    return 0;
}