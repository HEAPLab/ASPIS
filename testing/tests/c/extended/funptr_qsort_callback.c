#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static uint32_t checksum_u32(const int *a, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)a[i];
        h *= 16777619u;
    }
    return h;
}

int main(void) {
    const size_t N = 10;
    int *arr = malloc(N * sizeof(int));
    if (!arr) return 1;

    /* inizializzazione volutamente non ordinata */
    int init[10] = {7, 1, 9, 3, 5, 0, 8, 2, 6, 4};
    for (size_t i = 0; i < N; i++)
        arr[i] = init[i];

    /* NOVITÀ: callback a funzione di libreria */
    qsort(arr, N, sizeof(int), cmp_int);

    uint32_t cs = checksum_u32(arr, N);

    printf("checksum=%u\n", cs);
    printf("sentinels=%d %d %d %d\n",
           arr[0], arr[1], arr[N-2], arr[N-1]);

    free(arr);
    return 0;
}
