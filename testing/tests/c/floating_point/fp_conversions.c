/*
 * Floating-point conversions: values crossing between int, float and
 * double in both directions, each wrapped in its own function so the
 * conversion happens on a value passed as an argument.
 *
 * All checks are on exact values: the cases are chosen so the expected
 * result of each conversion is exactly representable, or the rounding
 * it causes is itself the property being asserted. 16777216 is the
 * largest integer that float can represent together with all the ones
 * below it; 16777217 falls exactly halfway between two representable
 * values and rounds to even, that is down. Conversion from double to
 * int truncates toward zero, also for negatives. Narrowing 0.1 to
 * float and back changes the value, so the check asserts inequality.
 *
 * The hardened counter makes the number of conversions observable: a
 * control-flow error that skips or repeats one alters the count even
 * when the returned value is unaffected. Each failed check sets its own
 * bit in the reported code.
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
unsigned long checks = 0;

/* int -> double -> int must round-trip for any int */
__attribute__((annotate("to_harden")))
int roundtrip_double(int v) {
    checks++;
    return (int)(double)v;
}

/* int -> float -> int loses integers above 2^24 */
__attribute__((annotate("to_harden")))
int through_float(int v) {
    checks++;
    return (int)(float)v;
}

/* double -> int truncates toward zero; named with a trailing underscore
   to avoid clashing with POSIX truncate() */
__attribute__((annotate("to_harden")))
int truncate_(double x) {
    checks++;
    return (int)x;
}

/* narrowing to float loses the tail of the mantissa */
__attribute__((annotate("to_harden")))
double narrow(double x) {
    checks++;
    return (double)(float)x;
}

int main(void) {
    int fail = 0;

    if (roundtrip_double(1000000)  != 1000000)  fail |= 1;
    if (roundtrip_double(-1000000) != -1000000) fail |= 2;
    if (through_float(16777216)    != 16777216) fail |= 4;
    if (through_float(16777217)    != 16777216) fail |= 8;
    if (truncate_(2.75)            != 2)        fail |= 16;
    if (truncate_(-2.75)           != -2)       fail |= 32;
    if (narrow(0.5)                != 0.5)      fail |= 64;
    if (narrow(0.1)                == 0.1)      fail |= 128;

    /* every conversion above ran exactly once */
    if (checks != 8)                              fail |= 256;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS