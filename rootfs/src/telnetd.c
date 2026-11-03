#include "nano_libc.h"

#define NETWORK_TTY_ID 4

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // 1. Setăm TTY 0 (ecranul fizic) pentru logurile daemonului
    nano_set_tty(0);
    nano_print("[TELNETSH] Pornire server Remote Shell...\n");

    int server_fd = socket(0, 0, 0);
    if (server_fd < 0) return 1;
    if (bind(server_fd, 23) < 0) return 1;
    if (listen(server_fd, 5) < 0) return 1;

    while (1) {
        int client_fd = accept(server_fd);
        if (client_fd < 0) continue;

        nano_set_tty(0);
        nano_print("[TELNETSH] Client conectat! Transfer I/O pe TTY 4.\n");

        // 2. Trecem pe TTY-ul virtual de rețea pentru restul sesiunii
        nano_set_tty(NETWORK_TTY_ID);
        nano_clear_screen();

        // 3. Magia Telnet: Oprim Ecoul Local și modul Linie din WSL
        // Codurile: IAC(255) WILL(251) ECHO(1), IAC(255) WILL(251) SGA(3), IAC(255) DO(253) SGA(3)
        //unsigned char telnet_init[] = { 255, 251, 1, 255, 251, 3, 255, 253, 3 };
        //send_socket(client_fd, (char*)telnet_init, sizeof(telnet_init));
		
		// 2. TRIMITEM SECVENȚA DE TELNET PENTRU OPRIREA ECOLOGULUI LOCAL:
        // - IAC WILL ECHO (255, 251, 1)   -> Serverul va face echo caracterelor
        // - IAC WONT ECHO (255, 252, 1)   sau cerem clientului să nu facă echo local
        // Standardul cel mai sigur pentru "Server-Side Echo":
        unsigned char telnet_disable_local_echo[] = {
            255, 251, 1,  // IAC WILL ECHO (Serverul preia responsabilitatea ecoului)
            255, 253, 1,  // IAC DO ECHO   (Cerem clientului să nu afișeze local)
            255, 251, 3,  // IAC WILL Suppress Go Ahead
            255, 253, 3   // IAC DO Suppress Go Ahead
        };
        send_socket(client_fd, (char*)telnet_disable_local_echo, sizeof(telnet_disable_local_echo));
		
        
        // Așteptăm puțin ca pachetul de configurare să plece spre WSL
        for (volatile int w = 0; w < 500000; w++);

        // 4. Lansăm shell-ul de sistem nativ
        int shell_pid = nano_exec("/sbin/shell");
        if (shell_pid <= 0) {
            close_socket(client_fd);
            continue;
        }

        // 5. Bucla Bridge (Puntea I/O)
        while (1) {
            char recv_buf[32];
            int len = recv_socket(client_fd, recv_buf, sizeof(recv_buf));
            
            if (len > 0) {
                for (int i = 0; i < len; i++) {
                    unsigned char c = (unsigned char)recv_buf[i];
                    
                    // Filtrăm/Ignorăm pachetele de control Telnet (IAC)
                    if (c == 255) {
                        if (i + 2 < len) i += 2;
                        else i = len - 1;
                        continue;
                    }
                    
                    // Normalizăm \r\n -> \n pentru shell
                    if (c == '\r') {
                        if (i + 1 < len && (unsigned char)recv_buf[i + 1] == '\n') i++;
                        c = '\n';
                    }
                    
                    nano_tty_inject(NETWORK_TTY_ID, (char)c);
                }
            } else if (len < 0) {
                break; // Client deconectat
            }

            char out_buf[256];
            int fetched = nano_read_tty_output(NETWORK_TTY_ID, out_buf, sizeof(out_buf));
            
            if (fetched > 0) {
                char send_buf[512];
                int send_len = 0;
                
                for (int k = 0; k < fetched; k++) {
                    if (out_buf[k] == '\n') {
                        send_buf[send_len++] = '\r';
                        send_buf[send_len++] = '\n';
                    } else if (out_buf[k] != '\r') {
                        send_buf[send_len++] = out_buf[k];
                    }
                }
                
                if (send_len > 0) {
                    send_socket(client_fd, send_buf, send_len);
                    
                    // ========================================================
                    // FIX-UL SUPREM PENTRU OVERWRITE-UL uIP (STIVA TCP):
                    // Îi dăm timp plăcii de rețea din kernel să proceseze 
                    // bufferul TX anterior înainte de a continua citirea!
                    // ========================================================
                    for (volatile int d = 0; d < 1000000; d++);
                    for (int y = 0; y < 15; y++) nano_yield();
                }
            }

            nano_yield(); // Păstrăm CPU-ul liber pentru shell și celelalte procese
        }

        close_socket(client_fd);
        nano_set_tty(0);
        nano_print("[TELNETSH] Sesiune remote închisă.\n");
    }
    
    return 0;
}