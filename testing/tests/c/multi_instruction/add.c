//
// Created by Gabriele Santandrea on 24/12/25.
//
#include <stdio.h>

__attribute__((annotate("to_harden")))
int a = 10;

int main() {
    int b = 20;
    int c = a + b;
    printf("%d\n", c);
    return 0;
}
