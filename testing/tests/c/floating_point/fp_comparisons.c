/*
 * Control flow driven by floating-point comparisons.
 *
 * classify() is called on 1000 samples x = i / 500.0, for i from 0 to
 * 999, and every branch it takes depends on a comparison between
 * doubles: an equality test and a chain of range tests. The samples hit
 * the boundaries 0.5, 1.0 and 1.5 exactly, since i / 500.0 is exactly
 * representable at those points, so both the < and the <= comparisons
 * are exercised on their edge values. A NaN sample would fail every
 * comparison and be counted as above.
 *
 * The result of the classification is a set of integer counters, so the
 * expected output is exact and needs no tolerance: 250 samples below
 * 0.5, 501 in [0.5, 1.5], 249 above 1.5, and exactly one equal to 1.0.
 * Each failed check sets its own bit in the reported code.
 */

#include <stdio.h>
#include <stdlib.h>

#define SAMPLES 1000

void DataCorruption_Handler(void) {
    fprintf(stderr, "DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "SigMismatch\n");
    exit(3);
}

__attribute__((annotate("to_harden")))
int below = 0;

__attribute__((annotate("to_harden")))
int inside = 0;

__attribute__((annotate("to_harden")))
int above = 0;

__attribute__((annotate("to_harden")))
int exact = 0;

__attribute__((annotate("to_harden")))
void classify(double x) {
    if (x == 1.0) {
        exact++;
    }
    if (x < 0.5) {
        below++;
    } else if (x <= 1.5) {
        inside++;
    } else {
        above++;
    }
}

int main(void) {
    for (int i = 0; i < SAMPLES; i++) {
        classify((double)i / 500.0);
    }

    int fail = 0;

    if (below != 250)                          fail |= 1;
    if (inside != 501)                         fail |= 2;
    if (above != 249)                          fail |= 4;
    if (exact != 1)                            fail |= 8;
    if (below + inside + above != SAMPLES)     fail |= 16;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS