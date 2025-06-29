#include <stdio.h>

int main() {
    int i = 5;
    float f = 2.5;
    char c = 3;
    long l = 4;

    float res = i + f + c + l; // 5 + 2.5 + 3 + 4 = 14.5
    printf("%.1f", res);
    return 0;
}

// expected output
// 14.5
