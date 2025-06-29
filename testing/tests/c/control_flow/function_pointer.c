/*
* Function pointer calls to validate inter-RASM control-flow checking.
* ASPIS should correctly handle duplicated calls via function pointers.
*/

#include <stdio.h>

int foo() {
    return 42;
}

int main() {
    int (*fptr)() = foo;
    int result = fptr();
    printf("%d", result);
    return 0;
}

// expected output
// 42
