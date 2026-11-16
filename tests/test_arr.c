#include "nano_libc.h"
#include <stdio.h>
int main(void) {
    int arr[5];
    int i = 0;

    // Populam tabloul
    for (i = 0; i < 5; i = i + 1) {
        arr[i] = (i + 1) * 10;
    }

    // Afisam elementele
    for (i = 0; i < 5; i = i + 1) {
        printf("%d",arr[i]);
    }

    print("------------\n");

    // Tablou de float-uri
    float farr[3];
    farr[0] = 1.1;
    farr[1] = 2.2;
    farr[2] = farr[0] + farr[1];

    printf("%d",farr[2]); // Va afisa 3.30
	
	//float* ff = &farr;
	float* ff = farr;
	print("------------\n");
	printf("%d",(*ff+1));
	printf("%f",ff[1]);
	
}