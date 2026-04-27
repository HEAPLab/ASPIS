//
// Created by martina on 28/12/25.
//
#include <stdio.h>

__attribute__((annotate("to_harden")))
int x = 1000;

int main() {
    x = x+1;
    if (x<10) {
        x = x*10;
        printf("%d\n", x);
    }
    else
        printf("%d\n", x);
    return 0;
}
