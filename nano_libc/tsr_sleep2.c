#include "nano_libc.h"

int main() {
    //nano_print("Pornesc...\n");
	for(;;){
    sleep(5000); // Așteaptă 3 secunde
    nano_print("Au trecut 5 secunde!\n");
    
	}
	
	return 0;
}