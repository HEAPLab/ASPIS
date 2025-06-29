/*
* Data duplication with branching logic. 
*/

#include <stdio.h>

int main() {
    int x = 5;
    int y = 0;

    if (x > 3) {
        y = 7;
    } else {
        y = 9;
    }

    printf("%d", y);
    return 0;
}

// expected output
// 7
