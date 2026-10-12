#ifndef __UIP_CONF_H__
#define __UIP_CONF_H__

#include <stdint.h>

#include "../../../kernel/string.h"
// Definirea tipului size_t pentru mediul freestanding x86-64
typedef uint64_t size_t;

// Tipurile de date de bază uIP
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint16_t uip_stats_t;

// Setări de bază uIP
#define UIP_CONF_BUFFER_SIZE     1500
#define UIP_CONF_MAX_CONNECTIONS 4
#define UIP_CONF_MAX_LISTENPORTS 4
#define UIP_CONF_UDP             0
#define UIP_CONF_STATISTICS      0
#define UIP_LOGGING              0
#define UIP_CONF_BYTE_ORDER      UIP_LITTLE_ENDIAN

// --- 1. APPLICATION STATE ȘI CALLBACK ---
typedef struct {
    int dummy;
} uip_tcp_appstate_t;

// Funcția pe care uIP o va apela atunci când se întâmplă evenimente TCP.
// O declarăm aici ca să știe compilatorul de jiul ei.
void nano_uip_appcall(void);
#define UIP_APPCALL nano_uip_appcall

// --- 2. EVITAREA HEADERELOR DE SISTEM (GLIBC) ---
#define _STRING_H  // Blochează includerea lui string.h de sistem

//void* memcpy(void* dest, const void* src, size_t n);
//void* memset(void* s, int c, size_t n);
//void* memcpy(void* dest, const void* src, int count);

#endif /* __UIP_CONF_H__ */