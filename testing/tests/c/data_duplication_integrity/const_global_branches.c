/*
* This test verifies that a hardened function can store the address of a
* constant global in one branch and pass the same global to a call in another
* branch: the temporary duplicate that REDDI creates for the global at the store
* must not be reused by the call, which it does not dominate.
*/

#include <stdio.h>

static const int values[] = {1, 2, 3};

int sum(const int *v, int count) {
    int s = 0;
    for (int i = 0; i < count; i++) {
        s += v[i];
    }
    return s;
}

__attribute__((annotate("to_harden")))
int pick(int argc) {
    const int *p = NULL;
    int count = 0;
    int n = 0;
    // Branch on a value not known at compile time
    if (argc > 1) {
        p = values;
        count = 3;
    } else {
        n = sum(values, 3);
    }
    return n + sum(p, count);
}

int main(int argc, char **argv) {
    printf("%d", pick(argc));
    return 0;
}

// expected output
// 6
