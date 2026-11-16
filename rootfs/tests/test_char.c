#include <stdio.h>
int main(void) {
    char c = 'A';
    char* ch = &c;

    printf("%c",ch);   // Afișează adresa
    printf("------------\n");
    printf("%c",*ch);  // Afișează 'A'

    *ch = 'Z';
    printf("%c",c);    // Afișează 'Z'
    printf("%c",*ch);  // Afișează 'Z'
}