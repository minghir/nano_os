#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

typedef struct {
    uint64_t fb_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
    int cols;
    int rows;
	uint32_t* back_buffer;
} VideoModeInfo;

// Funcții globale pentru gestionarea stării video
void video_init(uint64_t fb_addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
VideoModeInfo* video_get_info();
void video_swap_buffers();

#endif