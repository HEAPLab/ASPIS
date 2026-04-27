#include <stdio.h>
#include <stdlib.h>

__attribute__((annotate("to_harden")))
int a = 10;

int main() {
    int b = 20;
    if (a < b) {
        printf("OK");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// OK
