#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef void (*op_fn)(int *p, int n);

void op_add(int *p, int n) {
    for (int i = 0; i < n; i++) p[i] += 1;
}

void op_scale(int *p, int n) {
    for (int i = 0; i < n; i++) p[i] *= 2;
}

void op_xor(int *p, int n) {
    for (int i = 0; i < n; i++) p[i] ^= 0x5A;
}

static uint32_t checksum(const int *p, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
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
    const int N = 8;
    int *buf = dup_malloc(N * sizeof(int));
    if (!buf) return 1;

    for (int i = 0; i < N; i++) buf[i] = i + 1;

    int *alias = buf + 2;

    op_fn table[3] = { op_add, op_scale, op_xor };

    int idx = (buf[0] + buf[1]) % 3;

    op_fn f = table[idx];
    f(alias, N - 2);

    uint32_t h = checksum(buf, N);

    printf("checksum=%u\n", h);
    printf("sentinels=%d %d %d %d\n", buf[0], buf[1], buf[N-2], buf[N-1]);

    dup_free(buf);
    return 0;
}
