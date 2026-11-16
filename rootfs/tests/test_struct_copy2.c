#include <stdio.h>
struct Vec3 {
    int x;
    int y;
    int z;
};

int main() {
    struct Vec3 a;
    a.x = 10;
    a.y = 20;
    a.z = 30;

    struct Vec3 b = a;
    b.x = 99;

    struct Vec3 c;
    c = b;

    print(a.x);
    print(c.x);
    print(c.z);
    return 0;
}
