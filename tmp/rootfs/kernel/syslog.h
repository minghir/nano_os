#ifndef SYSLOG_H
#define SYSLOG_H

#define KERNEL_LOG_SIZE 4096
extern char kernel_log_buffer[KERNEL_LOG_SIZE];

// Tipurile de mesaje
enum msg_type {
    LOG_INFO = 0,
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

// Declarația funcției de bază
void kernel_log(const char* message, enum msg_type type);

// Macro-uri helper pentru fiecare tip în parte (cu paranteze!)
#define KLOG_INFO(msg)    kernel_log(msg, LOG_INFO)
#define KLOG_DEBUG(msg)   kernel_log(msg, LOG_DEBUG)
#define KLOG_WARNING(msg) kernel_log(msg, LOG_WARNING)
#define KLOG_ERROR(msg)   kernel_log(msg, LOG_ERROR)
#define KLOG_FATAL(msg)   kernel_log(msg, LOG_FATAL)

// Funcția care se apelează la CRASH / KERNEL PANIC
void kernel_panic(const char* reason);

#endif