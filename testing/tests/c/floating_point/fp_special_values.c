/*
 * IEEE-754 special values: NaN, infinities and negative zero.
 *
 * The special values are produced at run time from globals rather than
 * written as literals, so they cannot be folded away at compile time,
 * and they are then propagated through further arithmetic: 0/0 gives a
 * NaN, 1/0 and an overflowing multiplication give infinities, and
 * -1*0 gives negative zero.
 *
 * The properties asserted are those of IEEE-754 arithmetic. NaN is the
 * only value that compares unequal to itself, every ordered comparison
 * involving it is false, and it propagates through any arithmetic that
 * touches it. An infinity is non-zero and its reciprocal is exactly
 * zero. Negative zero compares equal to zero but keeps its sign, which
 * shows up in the sign of its reciprocal.
 *
 * The hardened counter makes the number of classification calls
 * observable: a control-flow error that skips or repeats one alters the
 * count even when the returned value is unaffected. All assertions are
 * exact, with no tolerance involved anywhere, and each failed check
 * sets its own bit in the reported code.
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
double zero = 0.0;

__attribute__((annotate("to_harden")))
double one = 1.0;

__attribute__((annotate("to_harden")))
double huge_val = 1.0e308;

__attribute__((annotate("to_harden")))
unsigned long checks = 0;

/* NaN is the only value that compares unequal to itself */
__attribute__((annotate("to_harden")))
int is_nan(double x) {
    checks++;
    return x != x;
}

/* an infinity is non-zero and its reciprocal is exactly zero */
__attribute__((annotate("to_harden")))
int is_inf(double x) {
    checks++;
    return x != 0.0 && one / x == 0.0;
}

int main(void) {
    double nan_v = zero / zero;        /* 0/0      -> NaN           */
    double pinf  = one / zero;         /* 1/0      -> +infinity     */
    double ninf  = -one / zero;        /* -1/0     -> -infinity     */
    double ovf   = huge_val * 10.0;    /* overflow -> +infinity     */
    double nzero = -one * zero;        /* -1*0     -> negative zero */

    int fail = 0;

    /* the three ways of reaching an infinity agree */
    if (!is_inf(pinf))          fail |=      1;
    if (!is_inf(ninf))          fail |=      2;
    if (!is_inf(ovf))           fail |=      4;
    if (!(pinf > 0.0))          fail |=      8;
    if (!(ninf < 0.0))          fail |=     16;

    /* NaN detection, and a normal value is not a NaN */
    if (!is_nan(nan_v))         fail |=     32;
    if (is_nan(one))            fail |=     64;

    /* NaN is produced by indeterminate forms and then propagates */
    if (!is_nan(pinf + ninf))   fail |=    128;
    if (!is_nan(pinf * zero))   fail |=    256;
    if (!is_nan(nan_v + one))   fail |=    512;

    /* negative zero compares equal to zero but keeps its sign */
    if (!(nzero == 0.0))          fail |=   1024;
    if (!is_inf(one / nzero))   fail |=   2048;
    if (!(one / nzero < 0.0))     fail |=   4096;

    /* every ordered comparison against NaN is false, != is true */
    if (nan_v < one)            fail |=   8192;
    if (nan_v > one)            fail |=  16384;
    if (nan_v == one)           fail |=  32768;
    if (!(nan_v != one))        fail |=  65536;

    /* every classification above ran exactly once */
    if (checks != 9)            fail |= 131072;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS