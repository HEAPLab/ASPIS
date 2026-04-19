/*
* Function pointer calls to validate inter-RASM control-flow checking.
* ASPIS should correctly handle duplicated calls via function pointers.
*/

#include <stdio.h>

int foo() {
    return 42;
}

int add(int a, int b) {
    return a + b;
}

int main() {
    // Simple function pointer call
    int (*fptr)() = foo;
    int result = fptr();
    printf("%d\n", result);

    // Function pointer call with parameters
    int (*addptr)(int, int) = add;
    int sum = addptr(27, result);
    printf("%d\n", sum);  
    return 0;
}

// expected output
// 42
