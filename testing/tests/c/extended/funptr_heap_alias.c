#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef void (*op_fn)(int *p, int n);

void add_seq(int *p, int n) {
    for (int i = 0; i < n; i++)
        p[i] += i;
}

void scale_seq(int *p, int n) {
    for (int i = 0; i < n; i++)
        p[i] *= 2;
}

uint32_t checksum(int *p, int n) {
    uint32_t h = 0;
    for (int i = 0; i < n; i++)
        h = (h * 33) ^ (uint32_t)p[i];
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

    for (int i = 0; i < N; i++)
        buf[i] = i + 1;

    int *alias1 = buf;
    int *alias2 = buf + 3;

    op_fn f1 = add_seq;
    op_fn f2 = scale_seq;

    f1(alias1, N);
    f2(alias2, N - 3);

    uint32_t h = checksum(buf, N);

    printf("checksum=%u\n", h);
    printf("sentinels=%d %d %d %d\n",
           buf[0], buf[1], buf[6], buf[7]);

    dup_free(buf);
    return 0;
}
