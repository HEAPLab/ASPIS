#include <stdio.h>

void add(int *x, int v) {
    *x += v;
}

void add_ptr(int **pp, int v) {
    **pp += v;
}

int main(void) {
    int a = 5;
    int *p = &a;
    int **pp = &p;

    add(&a, 3);    
    add_ptr(pp, 4);

    printf("VAL: %d\n", a);
    return 0;
}
