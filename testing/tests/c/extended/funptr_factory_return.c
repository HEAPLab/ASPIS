#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef void (*op_t)(int *p, size_t n);

static void op_add(int *p, size_t n) {
  for(size_t i = 0; i < n; i++) p[i] += (int)(i + 1);
}
static void op_xor(int *p, size_t n) {
  for(size_t i = 0; i < n; i++) p[i] ^= (int)(0x5A + (int)i);
}
static void op_mix(int *p, size_t n) {
  for(size_t i = 0; i < n; i++) p[i] = (p[i] * 3) - (int)i;
}

static op_t pick_op(uint32_t key) {
  switch(key % 3u) {
    case 0u: return &op_add;
    case 1u: return &op_xor;
    default: return &op_mix;
  }
}

static uint32_t checksum_u32(const int *a, size_t n) {
  uint32_t h = 2166136261u;
  for(size_t i = 0; i < n; i++) {
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
  const size_t N = 12;
  int *buf = (int*)dup_malloc(N * sizeof(int));
  if(!buf) return 1;

  for(size_t i = 0; i < N; i++) buf[i] = (int)(i * 7) - 3;

  int *alias = buf + 2;
  const size_t M = 6;

  uint32_t key = (uint32_t)(buf[1] * 31 + buf[4] * 17 + (int)N);

  op_t f = pick_op(key);
  f(alias, M);

  uint32_t cs = checksum_u32(buf, N);

  printf("checksum=%u\n", cs);
  printf("sentinels=%d %d %d %d\n", buf[0], buf[1], buf[N-2], buf[N-1]);

  dup_free(buf);
  return 0;
}
