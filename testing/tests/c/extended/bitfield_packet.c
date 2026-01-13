#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum {
    MSG_DATA = 0,
    MSG_ACK  = 1,
    MSG_ERR  = 2
} MsgType;

typedef struct {
    uint32_t type   : 2;
    uint32_t flags  : 6;
    uint32_t seq    : 12;
    uint32_t len    : 12;
} PacketHdr;

static void update(PacketHdr *h, int step) {
    h->flags ^= (uint32_t)(1u << (step & 5));
    h->seq   = (h->seq + 17u + (uint32_t)step) & 0xFFFu;
    if (h->type == MSG_DATA && h->len < 3000u) {
        h->len += 123u;
    } else {
        h->len = (h->len ^ 0x155u) & 0xFFFu;
    }
}

static uint32_t checksum_hdr(const PacketHdr *h) {
    uint32_t x = 2166136261u;
    x = (x ^ h->type)  * 16777619u;
    x = (x ^ h->flags) * 16777619u;
    x = (x ^ h->seq)   * 16777619u;
    x = (x ^ h->len)   * 16777619u;
    return x;
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
    PacketHdr *h = (PacketHdr *)dup_malloc(sizeof(PacketHdr));
    if (!h) return 1;

    h->type  = MSG_DATA;
    h->flags = 0x15u;
    h->seq   = 1000u;
    h->len   = 200u;

    for (int i = 0; i < 10; i++) {
        update(h, i);
    }

    uint32_t cs = checksum_hdr(h);

    printf("checksum=%u\n", cs);
    printf("sentinels=%u %u %u %u\n",
           (unsigned)h->type, (unsigned)h->flags, (unsigned)h->seq, (unsigned)h->len);

    dup_free(h);
    return 0;
}
