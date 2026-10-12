/*
 * Recursion carrying a pointer argument: every activation receives the
 * same pointer to a local variable of main() and writes through it
 * before recursing. The base case n == 0 makes no call; every other
 * activation makes one call, in tail position.
 * The accumulated value depends on every activation, so a control-flow
 * error that repeats or skips a call alters the result, and the final
 * check reports it as FAIL.
 *
 * sum_acc(100, &acc) adds 100 + 99 + ... + 1 = 5050 to acc, with 101
 * activations and a maximum recursion depth of 101.
 */

#include <stdio.h>
#include <stdlib.h>

void DataCorruption_Handler(void) {
    fprintf(stderr, "DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "SigMismatch\n");
    exit(3);
}

__attribute__((annotate("to_harden")))
void sum_acc(int n, int *acc) {
    if (n == 0) return;
    *acc += n;
    sum_acc(n - 1, acc);
}

int main(void) {
    int acc = 0;
    sum_acc(100, &acc);

    if (acc == 5050) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// SUCCESS