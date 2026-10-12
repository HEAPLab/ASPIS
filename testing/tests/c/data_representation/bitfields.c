/*
 * Bit-fields: several logical values packed inside one storage unit,
 * in the shape of a peripheral status register.
 *
 * A write to a single field is not a plain store, it is a
 * read-modify-write of the whole unit: load the word, mask out the
 * field, or in the new bits, store the word back. Each setter therefore
 * touches storage that belongs to fields it was not asked to change,
 * and the checks verify that the neighbouring fields keep their values.
 *
 * All fields are unsigned, since the representation of a signed
 * bit-field is implementation-defined. Assigning a value wider than the
 * field keeps only its low bits, which is also checked.
 *
 * The hardened counter makes the number of setter calls observable: a
 * control-flow error that skips or repeats one alters the count. Each
 * failed check sets its own bit in the reported code.
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

/* a status word of the kind found in a peripheral register */
struct status {
    unsigned ready   : 1;
    unsigned error   : 1;
    unsigned channel : 4;
    unsigned count   : 10;
};

__attribute__((annotate("to_harden")))
struct status reg;

__attribute__((annotate("to_harden")))
unsigned long writes = 0;

/* each setter touches one field and must leave the others alone */
__attribute__((annotate("to_harden")))
void set_channel(unsigned v) {
    writes++;
    reg.channel = v;
}

__attribute__((annotate("to_harden")))
void set_count(unsigned v) {
    writes++;
    reg.count = v;
}

__attribute__((annotate("to_harden")))
void bump(void) {
    writes++;
    reg.count = reg.count + 1;
}

int main(void) {
    int fail = 0;

    reg.ready = 1;
    reg.error = 0;
    set_channel(9);
    set_count(1000);

    /* writing every field left the earlier ones intact */
    if (reg.ready   != 1)     fail |=   1;
    if (reg.error   != 0)     fail |=   2;
    if (reg.channel != 9)     fail |=   4;
    if (reg.count   != 1000)  fail |=   8;

    /* 1000 + 23 = 1023, the largest value a 10-bit field holds */
    for (int i = 0; i < 23; i++) {
        bump();
    }
    if (reg.count != 0x3FF)   fail |=  16;

    /* a value wider than the field keeps only the low bits */
    set_channel(0x1F);        /* 4 bits:  0x1F  -> 0xF   */
    set_count(0x7FF);         /* 10 bits: 0x7FF -> 0x3FF */
    if (reg.channel != 0xF)   fail |=  32;
    if (reg.count   != 0x3FF) fail |=  64;

    /* the narrowing writes did not disturb the neighbouring fields */
    if (reg.ready != 1)       fail |= 128;
    if (reg.error != 0)       fail |= 256;

    /* every setter ran exactly once per call */
    if (writes != 27)         fail |= 512;

    if (fail == 0) {
        printf("SUCCESS");
    } else {
        printf("FAIL %d", fail);
    }
    return 0;
}

// expected output
// SUCCESS