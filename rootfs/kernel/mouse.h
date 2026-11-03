#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    uint8_t buttons; // Bit 0 = Stânga, Bit 1 = Dreapta, Bit 2 = Mijloc
} MouseState;

void mouse_init(void);
void mouse_update_position(int dx, int dy, uint8_t buttons);
void mouse_get_state(MouseState* state);

#endif
