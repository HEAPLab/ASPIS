#include <stdio.h>

__attribute__((annotate("to_harden")))
int sum = 0;

int main() {
    for (int i = 0; i < 3; i++) {
        int x = i * 2;
        if (x % 2 == 0) {
            sum += x;
        } else {
            sum -= x;
        }
    }
    printf("%d", sum);
    return 0;
}

// expected output
// 6
