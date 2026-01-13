#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t tag;
    uint32_t len;
    uint8_t  payload[64];
    uint32_t crc;
} Msg;

static uint32_t checksum_u32(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static void init_msg(Msg *m) {
    m->tag = 0xAABBCCDDu;
    m->len = 64u;
    for (uint32_t i = 0; i < m->len; i++) {
        m->payload[i] = (uint8_t)((i * 7u + 3u) & 0xFFu);
    }
    m->crc = checksum_u32(m->payload, m->len);
}

static void mutate_overlap(Msg *m) {
    memmove(&m->payload[5], &m->payload[0], 40);

    for (int i = 0; i < 10; i++) {
        m->payload[i] ^= (uint8_t)(0x5Au + (uint8_t)i);
    }

    m->crc = checksum_u32(m->payload, m->len);
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
    Msg *a = (Msg *)dup_malloc(sizeof(Msg));
    Msg *b = (Msg *)dup_malloc(sizeof(Msg));
    if (!a || !b) return 1;

    init_msg(a);

    memcpy(b, a, sizeof(Msg));

    mutate_overlap(b);

    uint32_t ca = checksum_u32(a->payload, a->len) ^ a->crc ^ a->tag ^ a->len;
    uint32_t cb = checksum_u32(b->payload, b->len) ^ b->crc ^ b->tag ^ b->len;

    printf("checksum=%u\n", (uint32_t)(ca * 31u + cb));
    printf("sentinels=%u %u %u %u\n",
           (unsigned)a->payload[0], (unsigned)a->payload[1],
           (unsigned)b->payload[0], (unsigned)b->payload[1]);

    dup_free(a);
    dup_free(b);
    return 0;
}
