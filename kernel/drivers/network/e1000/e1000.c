#include "e1000.h"
#include "../../../syslog.h"
#include "../../../memory.h"
#include "../../../paging.h"
#include "../../../string.h"

// --- Constante și Dimensiuni ---
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_MAX_PKT_SIZE 2048

// --- Registre e1000 ---
#define E1000_CTRL     0x0000
#define E1000_RAL      0x5400
#define E1000_RAH      0x5404
#define E1000_RCTL     0x0100
#define E1000_TCTL     0x0400
#define E1000_RDBAL    0x2800
#define E1000_RDBAH    0x2804
#define E1000_RDLEN    0x2808
#define E1000_RDH      0x2810
#define E1000_RDT      0x2818
#define E1000_TDBAL    0x3800
#define E1000_TDBAH    0x3804
#define E1000_TDLEN    0x3808
#define E1000_TDH      0x3810
#define E1000_TDT      0x3818

static volatile uint8_t* g_mmio_base = 0;

// --- Funcții ajutătoare MMIO (Definite PRIMUL) ---
static inline uint32_t e1000_read(uint16_t reg) {
    return *(volatile uint32_t*)(g_mmio_base + reg);
}

static inline void e1000_write(uint16_t reg, uint32_t data) {
    *(volatile uint32_t*)(g_mmio_base + reg) = data;
}

// Inelele și bufferele statice aliniate
static e1000_rx_desc* rx_descs = 0;
static e1000_tx_desc* tx_descs = 0;

static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_MAX_PKT_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_MAX_PKT_SIZE] __attribute__((aligned(16)));

static uint32_t tx_cur = 0;

// --- Inițializarea Ring-urilor RX / TX ---
static void e1000_init_rx_tx(void) {
    static e1000_rx_desc rx_ring_array[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
    static e1000_tx_desc tx_ring_array[E1000_NUM_TX_DESC] __attribute__((aligned(16)));

    rx_descs = rx_ring_array;
    tx_descs = tx_ring_array;

    // 1. RX Ring
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].address = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_descs[i].status = 0;
        rx_descs[i].errors = 0;
    }

    e1000_write(E1000_RDBAL, (uint32_t)(uintptr_t)rx_descs);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);

    // 2. TX Ring
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].address = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 0;
    }

    e1000_write(E1000_TDBAL, (uint32_t)(uintptr_t)tx_descs);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    // 3. Activare RCTL (Recepție)
    uint32_t rctl = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 15);
    e1000_write(E1000_RCTL, rctl);

    // 4. Activare TCTL (Transmisie)
    uint32_t tctl = (1 << 1) | (1 << 3) | (0x0F << 4) | (0x3FF << 12);
    e1000_write(E1000_TCTL, tctl);

    KLOG_INFO("[E1000] Inelele RX/TX au fost initializate si activate!\n");
}

static void e1000_init_impl(uint64_t mmio_base, __attribute__((unused)) uint8_t irq) {
    uint64_t* current_pml4;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_pml4));

    // Mapăm 128 KB de MMIO
    uint32_t mmio_size = 128 * 1024; 
    for (uint32_t offset = 0; offset < mmio_size; offset += 4096) {
        map_page(current_pml4,
                 mmio_base + offset,
                 mmio_base + offset,
                 PAGE_PRESENT | PAGE_WRITE);
    }

    g_mmio_base = (volatile uint8_t*)mmio_base;

    KLOG_INFO("[E1000] Initializare hardware Intel e1000...\n");

    if (g_mmio_base == 0) {
        KLOG_ERROR("[E1000] Erore: Adresa MMIO invalida!\n");
        return;
    }

    // Reset hardware
    uint32_t ctrl = e1000_read(E1000_CTRL);
    e1000_write(E1000_CTRL, ctrl | (1 << 26));
    
    for (volatile int i = 0; i < 100000; i++);

    // Set Link Up
    ctrl = e1000_read(E1000_CTRL);
    ctrl |= (1 << 6); 
    e1000_write(E1000_CTRL, ctrl);
    
    e1000_init_rx_tx();

    KLOG_INFO("[E1000] Placa de retea e1000 este pregatita!\n");
}

static void e1000_send_packet_impl(const uint8_t* data, uint32_t length) {
    KLOG_INFO("[E1000] Intraree in send_packet\n");
    if (g_mmio_base == 0 || length > E1000_MAX_PKT_SIZE) return;

    uint32_t idx = tx_cur;
    KLOG_INFO("[E1000] Copiere date in bufferul TX...\n");

    for (uint32_t i = 0; i < length; i++) {
        tx_buffers[idx][i] = data[i];
    }

    KLOG_INFO("[E1000] Setare descriptori TX...\n");
    tx_descs[idx].length = (uint16_t)length;
    tx_descs[idx].cmd = (1 << 0) | (1 << 1); // EOP + RS
    tx_descs[idx].status = 0;

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    
    KLOG_INFO("[E1000] Scriere in registrul TDT...\n");
    e1000_write(E1000_TDT, tx_cur);
    KLOG_INFO("[E1000] Pachet transmis cu succes prin e1000!\n");
}

static void e1000_get_mac_impl(uint8_t* mac_out) {
    if (g_mmio_base == 0) return;

    uint32_t low = e1000_read(E1000_RAL);
    uint32_t high = e1000_read(E1000_RAH);

    mac_out[0] = (low & 0xFF);
    mac_out[1] = (low >> 8) & 0xFF;
    mac_out[2] = (low >> 16) & 0xFF;
    mac_out[3] = (low >> 24) & 0xFF;
    mac_out[4] = (high & 0xFF);
    mac_out[5] = (high >> 8) & 0xFF;
}

static NetDriver e1000_driver = {
    .name = "Intel e1000",
    .init = e1000_init_impl,
    .send_packet = e1000_send_packet_impl,
    .get_mac = e1000_get_mac_impl
};

NetDriver* e1000_get_driver(void) {
    return &e1000_driver;
}