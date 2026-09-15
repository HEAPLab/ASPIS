/*
 * Floating-point accumulation: long chains of additions and divisions
 * in double precision, with no calls to libm.
 *
 * Two series are summed over 200000 terms each. The alternating one
 * (Leibniz, towards pi/4) changes sign at every term; the monotone one
 * (Basel, towards pi^2/6) adds terms that become far smaller than the
 * running total, where rounding affects the result. The expected
 * values are those of the truncated sums, not the limits of the series.
 *
 * The results are compared with a tolerance of 1e-9, written so that a
 * NaN result fails the check. The late terms of the monotone series are
 * smaller than the tolerance, so skipping or repeating one of them does
 * not move the sum enough to be noticed: the hardened counter makes the
 * number of iterations observable, and the final check reports such an
 * error as FAIL. Each failed check sets its own bit in the reported code.
 */

#include <stdio.h>
#include <stdlib.h>

#define TERMS 200000

void DataCorruption_Handler(void) {
    fprintf(stderr, "DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "SigMismatch\n");
    exit(3);
}

__attribute__((annotate("to_harden")))
unsigned long iterations = 0;

/* local absolute value: avoids depending on libm */
__attribute__((annotate("to_harden")))
double absd(double x) {
    return x < 0.0 ? -x : x;
}

/* Leibniz series: converges to pi/4 */
__attribute__((annotate("to_harden")))
double alternating(void) {
    double sum = 0.0;
    double sign = 1.0;
    for (int k = 0; k < TERMS; k++) {
        iterations++;
        sum += sign / (double)(2 * k + 1);
        sign = -sign;
    }
    return sum;
}

/* Basel series: converges to pi*pi/6 */
__attribute__((annotate("to_harden")))
double monotone(void) {
    double sum = 0.0;
    for (int k = 1; k <= TERMS; k++) {
        iterations++;
        sum += 1.0 / ((double)k * (double)k);
    }
    return sum;
}

int main(void) {
    double a = alternating();
    double b = monotone();

    int fail = 0;

    /* written as !(x < tol) so that a NaN result fails the check */
    if (!(absd(a - 0.785396913397) < 1e-9))  fail |= 1;
    if (!(absd(b - 1.644929066861) < 1e-9))  fail |= 2;
    if (iterations != 2 * TERMS)               fail |= 4;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS