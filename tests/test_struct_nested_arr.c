#include <stdio.h>
struct Vec2 {
    int x;
    int y;
};

struct Box {
    int id;
    struct Vec2 pos;
};

int main() {
    struct Vec2 v1;
    v1.x = 11;
    v1.y = 22;

    struct Vec2 v2;
    v2.x = 33;
    v2.y = 44;

    // 1. Test structură imbricată (obj.sub = v1)
    struct Box b;
    b.id = 1;
    b.pos = v1;

    // 2. Test prin pointer la structură (ptr->sub = v2)
    struct Box* ptr = &b;
    ptr->pos = v2;

    // 3. Test array de structuri (arr[i] = ...)
    struct Vec2 arr[2];
    arr[0] = v1;
    arr[1] = b.pos;

    print(b.pos.x);    // Trebuie să afișeze 33
    print(arr[0].y);   // Trebuie să afișeze 22
    print(arr[1].y);   // Trebuie să afișeze 44
    return 0;
}
