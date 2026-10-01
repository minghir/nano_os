#ifndef SYSLOG_H
#define SYSLOG_H

#define KERNEL_LOG_SIZE 4096
extern char kernel_log_buffer[KERNEL_LOG_SIZE];

void kernel_log(const char* message);
// Funcția care se apelează la CRASH / KERNEL PANIC
void kernel_panic(const char* reason);

#endif