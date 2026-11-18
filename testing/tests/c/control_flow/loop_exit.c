/*
* Validate correct handling of loop control flow and basic block transitions.
*/

#include <stdio.h>

void DataCorruption_Handler() {
    printf("ASPIS_FAULT_INJECTION_CAUGHT: DataCorruption_Handler\n");
    fflush(stdout);
}

void SigMismatch_Handler() {
    printf("ASPIS_FAULT_INJECTION_CAUGHT: SigMismatch_Handler\n");
    fflush(stdout);
}

__attribute__((annotate("to_harden")))
int sum = 0;

int main() {
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
