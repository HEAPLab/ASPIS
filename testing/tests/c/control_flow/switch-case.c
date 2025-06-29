/*
 * This test verifies that ASPIS correctly instruments control-flow
 * integrity for indirect jumps produced by a switch-case structure.
 */

#include <stdio.h>

int switch_test(int value) {
    switch (value) {
        case 0: return 100;
        case 1: return 200;
        case 2: return 250;
        case 3: return 300;
        case 4: return 400;
        default: return -1;
    }
}

int main() {
    int result = switch_test(3);
    printf("%d", result);
    return 0;
}


// expected output
// 300