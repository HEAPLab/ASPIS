/*
 * Tree recursion: every non-base activation makes two self-calls, so
 * the call graph is a binary tree. The recursion is not in tail
 * position: the result of the first call must survive across the
 * second one before the two are added.
 * The hardened counter makes the number of activations observable: a
 * control-flow error that repeats or skips a call alters the count
 * even when the returned value is unaffected, and the final check
 * reports it as FAIL.
 *
 * fib(20) = 6765. The number of activations is 2*F21 - 1 = 21891,
 * with a maximum recursion depth of 20.
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
unsigned long calls = 0;

__attribute__((annotate("to_harden")))
int fib(int n) {
    calls++;
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    int r = fib(20);

    if (r == 6765 && calls == 21891) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// SUCCESS