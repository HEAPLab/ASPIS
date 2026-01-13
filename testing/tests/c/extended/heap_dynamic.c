#include <stdio.h>
#include <stdlib.h>

__attribute__((annotate("to_duplicate")))
static void *dup_malloc(size_t n) {
    return malloc(n);
}

__attribute__((annotate("to_duplicate")))
static void dup_free(void *p) {
    free(p);
}

int main(void) {
    int *v = dup_malloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++)
        v[i] = i * 3;

    int sum = 0;
    for (int i = 0; i < 5; i++)
        sum += v[i];

    dup_free(v);

    printf("SUM: %d\n", sum);
    return 0;
}
