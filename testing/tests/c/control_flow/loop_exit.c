/*
* Validate correct handling of loop control flow and basic block transitions.
*/

#include <stdio.h>

int main() {
    int sum = 0;
    for (int i = 0; i < 5; i++) {
        if (i == 1) continue;
        if (i == 3) break;
        sum += i;
    }
    printf("%d", sum);
    return 0;
}

// expected output
// 2
