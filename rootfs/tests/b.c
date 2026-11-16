#include <stdio.h>
int main() {
    int x = 10;
    printf("Valoarea initiala a lui x:\n");
    printf("%d\d",x);
    
    // Reatribuire simplă
    x = 50;
    printf("Valoarea lui x dupa atribuire:\n");
    printf("%d\n",x);
    
    // Reatribuire cu expresie (x = x + 15)
    x = x + 15;
    printf("Valoarea lui x dupa x = x + 15:\n");
    printf("%d\n",x);
}