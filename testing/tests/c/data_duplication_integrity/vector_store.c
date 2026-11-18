/*
* This test verifies that values of vector type (e.g., <2 x i64> and <4 x float>)
* are compared correctly at the synchronization points, instead of being indexed
* like arrays.
*/

#include <stdio.h>

typedef long long v2i64 __attribute__((vector_size(16)));
typedef float v4f32 __attribute__((vector_size(16)));

__attribute__((annotate("to_harden")))
void compute(v2i64 *ints, v4f32 *floats) {
    *ints = *ints + *ints;
    *floats = *floats * *floats;
}

int main() {
    v2i64 ints = {1, 2};
    v4f32 floats = {0.5f, 1.5f, 2.5f, 3.5f};
    compute(&ints, &floats);
    printf("%lld %lld %.2f %.2f %.2f %.2f", ints[0], ints[1],
           floats[0], floats[1], floats[2], floats[3]);
    return 0;
}

// expected output
// 2 4 0.25 2.25 6.25 12.25
