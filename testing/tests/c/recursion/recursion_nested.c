/*
 * Nested recursion: the result of a self-call is used as the argument
 * of another self-call, so the depth of the recursion depends on the
 * values being computed. The function is Ackermann's: the base case
 * m == 0 makes no call, n == 0 makes one call in tail position, and the
 * general case makes an inner call whose result is passed to an outer
 * call in tail position.
 * The hardened counter makes the number of activations observable: a
 * control-flow error that repeats or skips a call alters the count,
 * and the final check reports it as FAIL.
 *
 * ack(3, 3) = 61, with 2432 activations and a maximum recursion depth
 * of 63.
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
int ack(int m, int n) {
    calls++;
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

int main(void) {
    int r = ack(3, 3);

    if (r == 61 && calls == 2432) {
        printf("SUCCESS");
    } else {
        printf("FAIL");
    }
    return 0;
}

// expected output
// SUCCESS