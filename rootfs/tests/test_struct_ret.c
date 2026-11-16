#include <stdio.h>
struct Vec3 {
    int x;
    int y;
    int z;
};

struct Vec3 make_vec(int a, int b, int c) {
    struct Vec3 res;
    res.x = a;
    res.y = b;
    res.z = c;
    return res;
}

struct Vec3 add_vec(struct Vec3* p1, struct Vec3* p2) {
    struct Vec3 sum;
    sum.x = p1->x + p2->x;
    sum.y = p1->y + p2->y;
    sum.z = p1->z + p2->z;
    return sum;
}

int main() {
    struct Vec3 v1 = make_vec(10, 20, 30);
    struct Vec3 v2;
    v2 = make_vec(5, 15, 25);

    struct Vec3 v3 = add_vec(&v1, &v2);

    print(v1.x);   // 10
    print(v2.y);   // 15
    print(v3.x);   // 15
    print(v3.z);   // 55
    return 0;
}
