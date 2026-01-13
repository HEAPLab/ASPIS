#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef int (*op_fn)(int);

static int op_add3(int x)  { return x + 3; }
static int op_xor55(int x) { return x ^ 0x55; }
static int op_mul7(int x)  { return x * 7; }
static int op_sub9(int x)  { return x - 9; }

static uint32_t mix_u32(uint32_t h, uint32_t x) {
  h ^= x + 0x9e3779b9u + (h << 6) + (h >> 2);
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

  int *a = (int*)dup_malloc(N * sizeof(int));
  if(!a) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }

  for(size_t i = 0; i < N; i++) {
    a[i] = (int)(i * 2) - 5;
  }

  op_fn ops[4] = { op_add3, op_xor55, op_mul7, op_sub9 };

  uint32_t checksum = 0x12345678u;

  for(size_t i = 0; i < N; i++) {
    op_fn f = ops[i & 3u];
    a[i] = f(a[i]);
    checksum = mix_u32(checksum, (uint32_t)a[i]);
  }

  int s0 = a[0];
  int s1 = a[1];
  int s2 = a[N/2];
  int s3 = a[N-1];

  dup_free(a);

  printf("checksum=%u\n", checksum);
  printf("sentinels=%d %d %d %d\n", s0, s1, s2, s3);
  return 0;
}
