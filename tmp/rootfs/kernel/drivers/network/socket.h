#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>

// Funcțiile de interfață pentru Syscall-uri
int kernel_create_socket(int domain, int type);
int kernel_socket_bind(int fd, uint16_t port);
int kernel_socket_listen(int fd, int backlog);
int kernel_socket_accept(int server_fd, void* current_process);
int kernel_socket_send(int fd, const char* buf, uint32_t len);
int kernel_socket_recv(int fd, char* buf, uint32_t max_len);
int kernel_socket_close(int fd);

void socket_subsystem_init(void);

#endif