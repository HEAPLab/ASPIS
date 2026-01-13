#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

static volatile sig_atomic_t got_signal = 0;

static void handler(int signum) {
    (void)signum;
    got_signal = 1;
}

static uint32_t checksum_u32(const int *a, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)a[i];
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
    const size_t N = 16;
    int *buf = (int *)dup_malloc(N * sizeof(int));
    if (!buf) return 1;

    for (size_t i = 0; i < N; i++)
        buf[i] = (int)(i * 3 + 7);

    if (signal(SIGUSR1, handler) == SIG_ERR) {
        dup_free(buf);
        return 2;
    }

    raise(SIGUSR1);

    if (got_signal) {
        for (size_t i = 0; i < N; i++)
            buf[i] ^= (int)(0x5A + (int)i);
    } else {
        for (size_t i = 0; i < N; i++)
            buf[i] += 11;
    }

    uint32_t cs = checksum_u32(buf, N);

    printf("checksum=%u\n", cs);
    printf("sentinels=%d %d %d %d\n", buf[0], buf[1], buf[N-2], buf[N-1]);
    printf("got_signal=%d\n", (int)got_signal);

    dup_free(buf);
    return 0;
}
