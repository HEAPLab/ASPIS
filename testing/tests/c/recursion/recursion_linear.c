/*
 * Linear recursion: at most one self-call per activation, in tail
 * position. The hardened counter makes the number of activations
 * observable: a control-flow error that repeats a call leaves the GCD
 * unchanged but alters the count, and the final check reports it as
 * FAIL.
 *
 * The inputs are consecutive Fibonacci numbers (F25, F24), the worst
 * case for Euclid's algorithm among inputs of that size: every quotient
 * is 1 and the remainder shrinks as slowly as possible. They are scaled
 * by 3 so that the expected GCD is 3 rather than 1, a value less likely
 * to arise by accident from a fault; the scaling does not change the
 * number of steps (24 activations).
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
int gcd(int a, int b) {
    calls++;
    if (b == 0) return a;
    return gcd(b, a % b);
}

int main(void) {
    int r = gcd(225075, 139104);

    if (r == 3 && calls == 24) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// SUCCESS