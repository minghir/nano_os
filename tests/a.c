#include "stdio.h"
int main() {
    int x = 42;
    int y = x + 18;
    
    printf("Testam logica compilatorului...\n");
    
    if (y >= 60) {
        printf("MIRACOL! Matematica a functionat corect!\n");
        printf("%d\n",y);
    }else{
		printf("Y nu e 60!\n");
	}
    
    if (y <= 100) {
		printf("Sub 100\n");
		if(y == 61){
			printf("Exact 60\n");
		}else{
			printf("nu e Exact 60\n");
		}
    }else{
		printf("Nu e 100!\n");
	}
}