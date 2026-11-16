#include <stdio.h>
#include "nano_libc.h"
int main() {
    int x = 0;
    
    print("Incepem numaratoarea:\n");
    
    while (x < 20) {
		if(x < 10)  printf("%d",x); 
		else { print("Mai mare ca 10");}
        x = x + 1;
		
    }
    
    print("Bucla s-a terminat cu succes!\n");
}