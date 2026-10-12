/*
 * Reductions over static arrays of float: a dot product and a maximum
 * over two global arrays of 512 elements.
 *
 * The arrays are filled at run time by fill() rather than initialised
 * statically, so the values reach the reductions through stores and
 * loads on the arrays.
 *
 * Every element and every product is a multiple of 0.25, and every
 * partial sum of the dot product is exactly representable in single
 * precision, so both results are exact and are checked by equality.
 * This keeps the expected output independent of FMA contraction and of
 * the target floating-point unit. Every term of the dot product is at
 * least 64, so skipping or repeating one changes the result; a NaN
 * result also fails the equality check. Each failed check sets its own
 * bit in the reported code.
 *
 * dot() = 2812608 and largest() = 128.
 */

#include <stdio.h>
#include <stdlib.h>

#define N 512

void DataCorruption_Handler(void) {
    fprintf(stderr, "DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "SigMismatch\n");
    exit(3);
}

__attribute__((annotate("to_harden")))
float xs[N];

__attribute__((annotate("to_harden")))
float ys[N];

__attribute__((annotate("to_harden")))
void fill(void) {
    for (int i = 0; i < N; i++) {
        xs[i] = (float)(i + 1) * 0.25f;
        ys[i] = (float)(N - i) * 0.5f;
    }
}

__attribute__((annotate("to_harden")))
float dot(void) {
    float acc = 0.0f;
    for (int i = 0; i < N; i++) {
        acc += xs[i] * ys[i];
    }
    return acc;
}

__attribute__((annotate("to_harden")))
float largest(void) {
    float m = xs[0];
    for (int i = 1; i < N; i++) {
        if (xs[i] > m) {
            m = xs[i];
        }
    }
    return m;
}

int main(void) {
    fill();
    float d = dot();
    float m = largest();

    int fail = 0;

    if (d != 2812608.0f)  fail |= 1;
    if (m != 128.0f)      fail |= 2;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS