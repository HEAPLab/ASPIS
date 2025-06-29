#include <stdio.h>

int main() {
    int a = 5, b = 3;
    int sum = a + b;
    int diff = sum - 2;
    int prod = diff * 4;
    int quot = prod / 3;
    int mod = quot % 5;
    printf("%d", mod); // expected result: ((((5+3)-2)*4)/3)%5 = (6*4)/3 = 24/3 = 8 % 5 = 3
    return 0;
}

// expected output
// 3
