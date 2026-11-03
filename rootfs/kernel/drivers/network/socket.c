#include "socket.h"
#include "../../syslog.h"
#include "../../memory.h"
#include "../../string.h"
#include "../../process.h"
#include "../../../thirdparty/uip/uip/uip.h"

#define MAX_KERNEL_SOCKETS 32

typedef enum {
    SOCKET_FREE = 0,
    SOCKET_BOUND,
    SOCKET_LISTENING,
    SOCKET_CONNECTED,
    SOCKET_CLOSED
} SocketState;

typedef struct {
    int id;
    SocketState state;
    uint16_t port;
    uint8_t rx_buffer[2048];
    uint16_t rx_len;
    uint8_t tx_buffer[2048];
    uint16_t tx_len;
    int has_pending_tx;
    PCB* listening_process;
    int has_pending_connection;
    int client_socket_id;
    struct uip_conn* conn;
} KernelSocket;

static KernelSocket socket_table[MAX_KERNEL_SOCKETS];

void socket_subsystem_init(void) {
    for (int i = 0; i < MAX_KERNEL_SOCKETS; i++) {
        socket_table[i].id = i;
        socket_table[i].state = SOCKET_FREE;
        socket_table[i].rx_len = 0;
        socket_table[i].tx_len = 0;
        socket_table[i].has_pending_tx = 0;
        socket_table[i].has_pending_connection = 0;
        socket_table[i].listening_process = 0;
        socket_table[i].conn = 0;
        socket_table[i].client_socket_id = -1;
    }
    KLOG_INFO("[SOCKET] Subsistemul de socket-uri din kernel a fost inițializat.\n");
}

int kernel_create_socket(int domain, int type) {
    (void)domain; (void)type;
    for (int i = 0; i < MAX_KERNEL_SOCKETS; i++) {
        if (socket_table[i].state == SOCKET_FREE) {
            socket_table[i].state = SOCKET_BOUND;
            socket_table[i].rx_len = 0;
            socket_table[i].tx_len = 0;
            socket_table[i].has_pending_tx = 0;
            socket_table[i].has_pending_connection = 0;
            socket_table[i].conn = 0;
            socket_table[i].client_socket_id = -1;
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "[SOCKET] Creat socket nou, asignat FD: %d\n", i);
            KLOG_INFO(log_msg);
            return i; 
        }
    }
    KLOG_INFO("[SOCKET] Eroare: Nu mai sunt sloturi libere de socket-uri!\n");
    return -1;
}

int kernel_socket_bind(int fd, uint16_t port) {
    if (fd < 0 || fd >= MAX_KERNEL_SOCKETS) return -1;
    if (socket_table[fd].state == SOCKET_FREE) return -1;
    
    socket_table[fd].port = port;
    
    char log_msg[64];
    snprintf(log_msg, sizeof(log_msg), "[SOCKET] Bind reusit pe FD %d la portul %d\n", fd, port);
    KLOG_INFO(log_msg);
    return 0;
}

int kernel_socket_listen(int fd, int backlog) {
    (void)backlog;
    if (fd < 0 || fd >= MAX_KERNEL_SOCKETS) return -1;
    if (socket_table[fd].state == SOCKET_FREE) return -1;

    socket_table[fd].state = SOCKET_LISTENING;
    uip_listen(htons(socket_table[fd].port));
    
    char log_msg[64];
    snprintf(log_msg, sizeof(log_msg), "[SOCKET] Socket-ul FD %d a trecut in starea LISTENING pe portul %d\n", fd, socket_table[fd].port);
    KLOG_INFO(log_msg);
    return 0;
}

int kernel_socket_accept(int server_fd, void* proc) {
    if (server_fd < 0 || server_fd >= MAX_KERNEL_SOCKETS) return -1;
    KernelSocket* server = &socket_table[server_fd];
    if (server->state != SOCKET_LISTENING) return -1;

    if (!server->has_pending_connection) {
        server->listening_process = (PCB*)proc;
        ((PCB*)proc)->state = PROC_SLEEPING;
        // KLOG_INFO("[SOCKET] Accept: Nici o conexiune noua, adormim procesul...\n");
        return -2; 
    }

    server->has_pending_connection = 0;
    int client_id = server->client_socket_id;
    if (client_id >= 0) {
        socket_table[client_id].state = SOCKET_CONNECTED;
    }
    
    KLOG_INFO("[SOCKET] Accept: Conexiune noua preluata cu succes!\n");
    return client_id;
}

int kernel_socket_send(int fd, const char* buf, uint32_t len) {
    if (fd < 0 || fd >= MAX_KERNEL_SOCKETS) return -1;
    KernelSocket* sock = &socket_table[fd];
    if (sock->state != SOCKET_CONNECTED) return -1;

    if (len > sizeof(sock->tx_buffer)) len = sizeof(sock->tx_buffer);

    for (uint32_t i = 0; i < len; i++) {
        sock->tx_buffer[i] = buf[i];
    }
    sock->tx_len = len;
    sock->has_pending_tx = 1;

    KLOG_INFO("[SOCKET] Date puse in bufferul TX pentru trimitere prin uIP.\n");
    return (int)len;
}

int kernel_socket_recv(int fd, char* buf, uint32_t max_len) {
    if (fd < 0 || fd >= MAX_KERNEL_SOCKETS) return -1;
    KernelSocket* sock = &socket_table[fd];
    
    if (sock->rx_len == 0) return 0;

    uint32_t to_copy = sock->rx_len < max_len ? sock->rx_len : max_len;
    for (uint32_t i = 0; i < to_copy; i++) {
        buf[i] = (char)sock->rx_buffer[i];
    }
    
    sock->rx_len = 0;
    return (int)to_copy;
}

int kernel_socket_close(int fd) {
    if (fd < 0 || fd >= MAX_KERNEL_SOCKETS) return -1;
    socket_table[fd].state = SOCKET_FREE;
    socket_table[fd].rx_len = 0;
    socket_table[fd].tx_len = 0;
    socket_table[fd].has_pending_tx = 0;
    KLOG_INFO("[SOCKET] Socket inchis.\n");
    return 0;
}

// --- BRIDGE-UL CĂTRE uIP ---
void socket_uip_appcall(void) {
    uint16_t lport = ntohs(uip_conn->lport);

    KernelSocket* server_sock = NULL;
    for (int i = 0; i < MAX_KERNEL_SOCKETS; i++) {
        if (socket_table[i].state == SOCKET_LISTENING && socket_table[i].port == lport) {
            server_sock = &socket_table[i];
            break;
        }
    }

    KernelSocket* client_sock = NULL;
    for (int i = 0; i < MAX_KERNEL_SOCKETS; i++) {
        if (socket_table[i].conn == uip_conn) {
            client_sock = &socket_table[i];
            break;
        }
    }

    if (server_sock && uip_connected()) {
        KLOG_INFO("[uIP] Eveniment UIP_CONNECTED detectat pe portul ascultator!\n");
        int client_fd = -1;
        for (int i = 0; i < MAX_KERNEL_SOCKETS; i++) {
            if (socket_table[i].state == SOCKET_FREE) {
                socket_table[i].state = SOCKET_CONNECTED;
                socket_table[i].conn = uip_conn;
                socket_table[i].rx_len = 0;
                socket_table[i].tx_len = 0;
                socket_table[i].has_pending_tx = 0;
                client_fd = i;
                break;
            }
        }

        if (client_fd != -1) {
            server_sock->client_socket_id = client_fd;
            server_sock->has_pending_connection = 1;

            if (server_sock->listening_process) {
                server_sock->listening_process->state = PROC_READY;
                server_sock->listening_process = NULL;
                KLOG_INFO("[uIP] Procesul din user-space trezit din accept()!\n");
            }
        }
    }

    if (client_sock) {
        if (uip_newdata()) {
            KLOG_INFO("[uIP] Date noi primite de la client pe socket!\n");
            uint16_t datalen = uip_len;
            if (datalen > sizeof(client_sock->rx_buffer)) {
                datalen = sizeof(client_sock->rx_buffer);
            }
            for (uint16_t j = 0; j < datalen; j++) {
                client_sock->rx_buffer[j] = ((uint8_t*)uip_appdata)[j];
            }
            client_sock->rx_len = datalen;
        }

        if (client_sock->has_pending_tx && client_sock->tx_len > 0) {
            KLOG_INFO("[uIP] Trimitere date în asteptare prin uip_send...\n");
            uip_send(client_sock->tx_buffer, client_sock->tx_len);
            client_sock->has_pending_tx = 0;
            client_sock->tx_len = 0;
        }

        if (uip_closed() || uip_aborted()) {
            KLOG_INFO("[uIP] Conexiune închisă de la distanță sau abortată.\n");
            client_sock->state = SOCKET_FREE;
            client_sock->conn = NULL;
        }
    }
}