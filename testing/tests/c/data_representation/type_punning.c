/*
 * Type punning through unions: the same storage read back as a
 * different type from the one it was written as. A float is read as its
 * bit pattern and back, a word is read as an array of four bytes, and
 * as a pair of half words.
 *
 * Reading a union member other than the one last written is defined in
 * C and is supported as a documented extension by both clang and gcc
 * when the file is compiled as C++, which is how the reference build
 * of the test suite compiles it.
 *
 * Every assertion is on an integer bit pattern, on a sum of bytes or on
 * a sum of half words, so nothing here depends on byte order or on the
 * target floating-point unit: 1.0f is 0x3F800000 in IEEE-754 single
 * precision whatever the endianness, and a sum does not care in which
 * order its terms are stored.
 *
 * The hardened counter makes the number of punning calls observable: a
 * control-flow error that skips or repeats one alters the count even
 * when the returned value is unaffected. Each failed check sets its own
 * bit in the reported code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void DataCorruption_Handler(void) {
    fprintf(stderr, "DataCorruption\n");
    exit(2);
}

void SigMismatch_Handler(void) {
    fprintf(stderr, "SigMismatch\n");
    exit(3);
}

union word {
    uint32_t u;
    float    f;
    uint8_t  b[4];
};

union halves {
    uint32_t whole;
    uint16_t part[2];
};

__attribute__((annotate("to_harden")))
unsigned long checks = 0;

__attribute__((annotate("to_harden")))
uint32_t float_to_bits(float v) {
    union word w;
    checks++;
    w.f = v;
    return w.u;
}

__attribute__((annotate("to_harden")))
float bits_to_float(uint32_t v) {
    union word w;
    checks++;
    w.u = v;
    return w.f;
}

/* summing the bytes is independent of the order they are stored in */
__attribute__((annotate("to_harden")))
unsigned byte_sum(uint32_t v) {
    union word w;
    unsigned s = 0;
    checks++;
    w.u = v;
    for (int i = 0; i < 4; i++) {
        s += w.b[i];
    }
    return s;
}

/* the two halves must add up to the whole, whichever end they sit at */
__attribute__((annotate("to_harden")))
uint32_t halves_sum(uint32_t v) {
    union halves h;
    checks++;
    h.whole = v;
    return (uint32_t)h.part[0] + (uint32_t)h.part[1];
}

int main(void) {
    int fail = 0;

    /* a float written, then read back as its bit pattern */
    if (float_to_bits(1.0f)        != 0x3F800000u) fail |=   1;
    if (float_to_bits(-1.0f)       != 0xBF800000u) fail |=   2;
    if (float_to_bits(0.0f)        != 0x00000000u) fail |=   4;

    /* and the same trip in the opposite direction */
    if (bits_to_float(0x40000000u) != 2.0f)        fail |=   8;
    if (bits_to_float(0x3F800000u) != 1.0f)        fail |=  16;

    /* the same storage seen as an array of bytes */
    if (byte_sum(0x3F800000u)      != 0x3F + 0x80) fail |=  32;
    if (byte_sum(0xFFFFFFFFu)      != 4 * 255)     fail |=  64;

    /* and as a pair of half words */
    if (halves_sum(0x00010002u)    != 0x0003)      fail |= 128;

    /* every punning function ran exactly once */
    if (checks != 8)                                 fail |= 256;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS