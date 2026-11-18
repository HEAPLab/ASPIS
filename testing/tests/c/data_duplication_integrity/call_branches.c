/*
* This test verifies that a non-hardened function can pass the same local
* variable to a hardened function from two different branches: the temporary
* duplicate that REDDI creates for the argument of one call must not be reused
* by the other call, which it does not dominate.
*/

#include <stdio.h>

__attribute__((annotate("to_harden")))
void add_one(int *x) {
    *x += 1;
}

int main(int argc, char **argv) {
    int x = 1;
    // Branch on a value not known at compile time
    if (argc > 1) {
        add_one(&x);
    } else {
        add_one(&x);
        add_one(&x);
    }
    printf("%d", x);
    return 0;
}

// expected output
// 3
