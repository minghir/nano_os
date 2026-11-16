#include <stdio.h>
int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

int main() {
    int res = sum6(10, 20, 30, 40, 50, 60);
    printf("%d",res); // trebuie sa afiseze 210
    return 0;
}