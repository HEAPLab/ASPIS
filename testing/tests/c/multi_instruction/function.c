//
// Created by Gabriele Santandrea
//
#include <stdio.h>

__attribute__((annotate("to_harden")))
int foo();
void print(int c);

int main() {
    int a = 10;
    int b = 20;
    int c = foo();
    print(c);
    return a > b ? 1 : 0;
}

__attribute__((annotate("to_harden")))
int foo() {
    int c = 12;
    int d = 13;
    return c + d;
}

void print(int c) {
    printf("foo() %d\n", c);
}
