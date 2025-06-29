#include <stdio.h>

int main() {
    int sum = 0;
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
