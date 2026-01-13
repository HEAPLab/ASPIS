#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

typedef struct {
    int *buf;
    int n;
} Task;

static void op_add(int *buf, int n) {
    for (int i = 0; i < n; i++)
        buf[i] += i;
}

static uint32_t checksum(int *buf, int n) {
    uint32_t c = 0;
    for (int i = 0; i < n; i++)
        c = c * 31u + (uint32_t)buf[i];
    return c;
}

__attribute__((annotate("exclude")))
static void *worker(void *arg) {
    Task *t = (Task *)arg;
    op_add(t->buf, t->n);
    return NULL;
}

int main(void) {
    const int n = 8;
    int *buf = malloc(n * sizeof(int));
    if (!buf) return 1;

    for (int i = 0; i < n; i++)
        buf[i] = i + 1;

    Task t = { buf, n };

    pthread_t th;
    if (pthread_create(&th, NULL, worker, &t) != 0) {
        free(buf);
        return 2;
    }
    pthread_join(th, NULL);

    uint32_t cs = checksum(buf, n);

    printf("checksum=%u\n", cs);
    printf("sentinels=%d %d %d %d\n", buf[0], buf[1], buf[n-2], buf[n-1]);

    free(buf);
    return 0;
}
