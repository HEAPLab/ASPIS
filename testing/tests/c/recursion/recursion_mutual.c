/*
 * Mutual recursion: is_even() and is_odd() call each other, so the
 * recursive cycle crosses a function boundary at every activation.
 * Every non-base activation makes one call, in tail position.
 * The hardened counter makes the number of activations observable: a
 * control-flow error that repeats or skips a call alters the count,
 * and the final check reports it as FAIL.
 *
 * is_even(30) and is_odd(30) each take 31 activations and return 1 and
 * 0 respectively, for 62 activations in total and a maximum recursion
 * depth of 31.
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

int is_odd(int n);

__attribute__((annotate("to_harden")))
int is_even(int n) {
    calls++;
    if (n == 0) return 1;
    return is_odd(n - 1);
}

__attribute__((annotate("to_harden")))
int is_odd(int n) {
    calls++;
    if (n == 0) return 0;
    return is_even(n - 1);
}

int main(void) {
    int a = is_even(30);
    int b = is_odd(30);

    if (a == 1 && b == 0 && calls == 62) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// SUCCESS