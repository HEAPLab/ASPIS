#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

static uint32_t checksum_u32(const uint32_t *a, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= a[i];
        h *= 16777619u;
    }
    return h;
}

__attribute__((annotate("to_duplicate")))
static void* dup_malloc(size_t n) {
    return malloc(n);
}

__attribute__((annotate("to_duplicate")))
static void dup_free(void* p) {
    free(p);
}

int main(void) {
    const size_t N = 64;

    uint32_t *buf = (uint32_t *)dup_malloc(N * sizeof(uint32_t));
    if (!buf) return 1;

    for (size_t i = 0; i < N; i++)
        buf[i] = (uint32_t)(i * 3u + 1u);

    _Atomic uint32_t ctr = 0;

    for (uint32_t i = 0; i < 10000; i++) {
        uint32_t v = atomic_fetch_add_explicit(&ctr, 1u, memory_order_relaxed);
        size_t idx = (size_t)(v % N);
        buf[idx] ^= (v * 2654435761u) ^ (uint32_t)idx;
    }

    uint32_t cs = checksum_u32(buf, N);

    printf("checksum=%u\n", cs);
    printf("sentinels=%u %u %u %u\n", buf[0], buf[1], buf[N-2], buf[N-1]);
    printf("ctr=%u\n", atomic_load_explicit(&ctr, memory_order_relaxed));

    dup_free(buf);
    return 0;
}
