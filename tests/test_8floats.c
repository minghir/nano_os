#include <stdio.h>
float sum8f(float a, float b, float c, float d, float e, float f, float g, float h) {
    return a + b + c + d + e + f + g + h;
}

int main() {
    float res = sum8f(1.5, 2.5, 3.0, 4.0, 5.0, 6.0, 7.0, 8.5);
    printf("%f",res); // 1.5 + 2.5 + 3 + 4 + 5 + 6 + 7 + 8.5 = 37.5
    return 0;
}