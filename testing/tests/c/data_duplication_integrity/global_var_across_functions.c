/* 
* Duplicated variable across functions.
*/

#include <stdio.h>

__attribute__((annotate("to_harden")))
int g = 0;

void increment() {
    g += 1;
}

void print() {
    printf("%d", g);
}

int main() {
    increment();
    increment();
    print();
    return 0;
}

// expected output
// 2
