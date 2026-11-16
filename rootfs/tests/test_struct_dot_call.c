#include <stdio.h>
struct Vec {
    int x;
    int y;
    int z;
};

struct Line {
    struct Vec start;
    struct Vec end;
};

struct Vec make_vec(int x, int y, int z) {
    struct Vec v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

struct Line make_line(struct Vec* p1, struct Vec* p2) {
    struct Line l;
    l.start = *p1;
    l.end = *p2;
    return l;
}

struct Vec* get_vec_ptr(struct Vec* p) {
    return p;
}

int main() {
    // 1. Acces direct pe apel (trebuie sa afiseze 10 si 30)
    int a = make_vec(10, 20, 30).x;
    int b = make_vec(10, 20, 30).z;
    print(a);
    print(b);

    // 2. Doua apeluri in aceeasi expresie: 5 + 15 = 20
    int suma = make_vec(5, 6, 7).x + make_vec(14, 15, 16).y;
    print(suma);

    // 3. Structuri imbricate pe apel: make_line(&p1, &p2).end.y -> 400
    struct Vec p1 = make_vec(100, 200, 0);
    struct Vec p2 = make_vec(300, 400, 0);
    int nested = make_line(&p1, &p2).end.y;
    print(nested);

    // 4. Acces cu '->' pe functie care returneaza pointer -> 30
    struct Vec v = make_vec(10, 20, 30);
    int ptr_val = get_vec_ptr(&v)->z;
    print(ptr_val);
	
	// 5. Scriere directă prin pointer returnat de funcție
    get_vec_ptr(&v)->x = 99;
    get_vec_ptr(&v)->z = 777;
    print(v.x); // trebuie sa afiseze 99
    print(v.z); // trebuie sa afiseze 777

    return 0;
}