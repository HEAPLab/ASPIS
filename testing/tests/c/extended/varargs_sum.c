#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

static int v_sum(int count, ...) {
    va_list ap;
    va_start(ap, count);

    int s = 0;
    for (int i = 0; i < count; i++) {
        s += va_arg(ap, int);
    }

    va_end(ap);
    return s;
}

static uint32_t checksum_u32(const int *a, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) {
        h ^= (uint32_t)a[i];
        h *= 16777619u;
    }
    return h;
}

int main(void) {
    const int n = 8;
    int *buf = malloc(n * sizeof(int));
    if (!buf) return 1;

    for (int i = 0; i < n; i++)
        buf[i] = i + 1;  /* 1..8 */

    /* usiamo varargs per calcolare un offset deterministico */
    int off = v_sum(4, buf[0], buf[1], buf[2], buf[3]); /* 1+2+3+4 = 10 */

    for (int i = 0; i < n; i++)
        buf[i] += off; /* +10 su tutti */

    uint32_t cs = checksum_u32(buf, n);

    printf("checksum=%u\n", cs);
    printf("sentinels=%d %d %d %d\n", buf[0], buf[1], buf[n-2], buf[n-1]);

    free(buf);
    return 0;
}
