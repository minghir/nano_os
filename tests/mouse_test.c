#include "nano_libc.h"

int main(int argc, char** argv) {
    
    
    MouseState mouse;
    
    while (1) {
      
        // 2. Citim starea mouse-ului prin Syscall
        if (nano_get_mouse_state(&mouse)) {
			nano_printf("x=%d | y=%d \n", mouse.x, mouse.y);
		}
      
		}
    return 0;
}