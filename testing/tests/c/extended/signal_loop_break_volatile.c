#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

static volatile sig_atomic_t stop_flag = 0;
static volatile uint32_t tick = 0;

static void handler(int signum) {
    (void)signum;
    stop_flag = 1;
    tick ^= 0xA5A5A5A5u;
}

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
    const size_t N = 32;
    uint32_t *buf = (uint32_t *)dup_malloc(N * sizeof(uint32_t));
    if (!buf) return 1;

    for (size_t i = 0; i < N; i++)
        buf[i] = (uint32_t)(i * 11u + 7u);

    if (signal(SIGUSR1, handler) == SIG_ERR) {
        dup_free(buf);
        return 2;
    }

    int iters = 0;
    for (int i = 0; i < 100000; i++) {
        iters++;

        tick = (tick * 1664525u) + 1013904223u;
        buf[(size_t)i % N] ^= tick;

        if (i == 200) {
            raise(SIGUSR1);
        }

        if (stop_flag) {
            break;
        }
    }

    uint32_t cs = checksum_u32(buf, N);

    printf("checksum=%u\n", cs);
    printf("sentinels=%u %u %u %u\n", buf[0], buf[1], buf[N-2], buf[N-1]);
    printf("stop_flag=%d iters=%d tick=%u\n", (int)stop_flag, iters, (uint32_t)tick);

    dup_free(buf);
    return 0;
}
