#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>

/* ---------------- checksum helper (FNV-1a like) ---------------- */
static uint32_t fnv1a_u32(uint32_t h, uint32_t x) {
  h ^= x;
  h *= 16777619u;
  return h;
}
static uint32_t hash_mix_u32(uint32_t h, const uint32_t *a, size_t n) {
  for(size_t i = 0; i < n; i++) h = fnv1a_u32(h, a[i]);
  return h;
}

/* ---------------- recursion (known OK) ---------------- */
static int factorial(int n) {
  if(n < 0) return -1;
  if(n == 0 || n == 1) return 1;
  return n * factorial(n - 1);
}

/* ---------------- 2D array (known OK) ---------------- */
static int sum2d_5(int a[5][5]) {
  int s = 0;
  for(int i = 0; i < 5; i++)
    for(int j = 0; j < 5; j++)
      s += a[i][j];
  return s;
}

/* ---------------- ptr-to-ptr (known OK) ---------------- */
static int diag_sum_pp(int **pp, int n) {
  int s = 0;
  for(int i = 0; i < n; i++) s += pp[i][i];
  return s;
}

/* ---------------- bitfield packet (known OK) ---------------- */
typedef struct {
  unsigned type : 3;
  unsigned flags: 5;
  unsigned len  : 12;
  unsigned id   : 12;
} PacketBits;

static uint32_t pack_bits(PacketBits p) {
  uint32_t x = 0;
  x |= (p.type  & 0x7u);
  x |= (p.flags & 0x1Fu) << 3;
  x |= (p.len   & 0xFFFu) << 8;
  x |= (p.id    & 0xFFFu) << 20;
  return x;
}
static PacketBits unpack_bits(uint32_t x) {
  PacketBits p;
  p.type  = (x      ) & 0x7u;
  p.flags = (x >>  3) & 0x1Fu;
  p.len   = (x >>  8) & 0xFFFu;
  p.id    = (x >> 20) & 0xFFFu;
  return p;
}

/* ---------------- memcpy bulk on structs (known OK) ---------------- */
typedef struct {
  int32_t a;
  int32_t b;
  uint32_t tag;
} Rec;

static uint32_t bulk_struct_op(void) {
  Rec src[8];
  Rec dst[8];
  for(int i = 0; i < 8; i++) {
    src[i].a = (int32_t)(i * 7 - 3);
    src[i].b = (int32_t)(i * i + 11);
    src[i].tag = 0xABC000u + (uint32_t)i;
  }
  memcpy(dst, src, sizeof(src));
  for(int i = 0; i < 8; i++) {
    dst[i].a ^= (int32_t)dst[i].tag;
    dst[i].b += dst[i].a;
  }
  uint32_t h = 2166136261u;
  for(int i = 0; i < 8; i++) {
    h = fnv1a_u32(h, (uint32_t)dst[i].a);
    h = fnv1a_u32(h, (uint32_t)dst[i].b);
    h = fnv1a_u32(h, dst[i].tag);
  }
  return h;
}

/* ---------------- heap + alias + function pointer dispatch (known OK) ---------------- */
typedef void (*op_fn)(uint32_t *buf, size_t n);

static void op_add(uint32_t *buf, size_t n) {
  for(size_t i = 0; i < n; i++) buf[i] += (uint32_t)i;
}
static void op_xor(uint32_t *buf, size_t n) {
  for(size_t i = 0; i < n; i++) buf[i] ^= (0x9E3779B9u ^ (uint32_t)(i * 13u));
}
static void op_mix(uint32_t *buf, size_t n) {
  for(size_t i = 0; i < n; i++) buf[i] = (buf[i] * 33u) ^ (buf[i] >> 1);
}

static op_fn pick_op(uint32_t key) {
  switch(key % 3u) {
    case 0: return op_add;
    case 1: return op_xor;
    default: return op_mix;
  }
}

/* ---------------- signals (known OK) ---------------- */
static volatile sig_atomic_t got_signal = 0;
static volatile sig_atomic_t stop_flag = 0;

static void on_sigusr1(int signum) {
  (void)signum;
  got_signal = 1;
}
static void on_sigusr2(int signum) {
  (void)signum;
  stop_flag = 1;
}

/* ---------------- atomics (known OK) ---------------- */
static uint32_t atomic_stress(void) {
  _Atomic uint32_t ctr = 0;
  const size_t N = 32;
  uint32_t buf[N];
  for(size_t i = 0; i < N; i++) buf[i] = (uint32_t)(i * 3u + 1u);

  for(uint32_t i = 0; i < 2000; i++) {
    uint32_t v = atomic_fetch_add_explicit(&ctr, 1u, memory_order_relaxed);
    size_t idx = (size_t)(v % N);
    buf[idx] ^= (v * 2654435761u) ^ (uint32_t)idx;
  }

  uint32_t h = 2166136261u;
  h = hash_mix_u32(h, buf, N);
  h = fnv1a_u32(h, atomic_load_explicit(&ctr, memory_order_relaxed));
  return h;
}

/* ---------------- malloc/free wrappers (README: to_duplicate) ---------------- */
__attribute__((annotate("to_duplicate")))
static void* dup_malloc(size_t n) {
  return malloc(n);
}

__attribute__((annotate("to_duplicate")))
static void dup_free(void* p) {
  free(p);
}

/* ---------------- main: glue everything ---------------- */
int main(void) {
  uint32_t H = 2166136261u;

  int f10 = factorial(10);
  H = fnv1a_u32(H, (uint32_t)f10);

  int a[5][5];
  int v = 1;
  for(int i = 0; i < 5; i++)
    for(int j = 0; j < 5; j++)
      a[i][j] = v++;
  int s2d = sum2d_5(a);
  H = fnv1a_u32(H, (uint32_t)s2d);

  int m[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
  int *rows[3] = { m[0], m[1], m[2] };
  int **pp = rows;
  int d1 = diag_sum_pp(pp, 3);
  m[1][1] += 10;
  int d2 = diag_sum_pp(pp, 3);
  H = fnv1a_u32(H, (uint32_t)d1);
  H = fnv1a_u32(H, (uint32_t)d2);

  PacketBits p = { .type = 6, .flags = 22, .len = 1215, .id = 1430 };
  uint32_t packed = pack_bits(p);
  PacketBits q = unpack_bits(packed);
  H = fnv1a_u32(H, packed);
  H = fnv1a_u32(H, (uint32_t)(q.type | (q.flags<<8) | (q.len<<16)));

  uint32_t h_bulk = bulk_struct_op();
  H = fnv1a_u32(H, h_bulk);

  const size_t N = 64;
  uint32_t *buf = (uint32_t*)dup_malloc(N * sizeof(uint32_t));
  if(!buf) return 1;
  for(size_t i = 0; i < N; i++) buf[i] = (uint32_t)(i * 7u + 1u);

  uint32_t *alias = buf + 8;
  op_fn table[3] = { op_add, op_xor, op_mix };

  table[0](alias, N - 8);
  table[1](alias, N - 8);

  uint32_t key = buf[0] ^ buf[1] ^ buf[2];
  op_fn f = pick_op(key);
  f(alias, N - 8);

  signal(SIGUSR1, on_sigusr1);
  signal(SIGUSR2, on_sigusr2);

  raise(SIGUSR1);
  H = fnv1a_u32(H, (uint32_t)got_signal);

  uint32_t tick = 0x12345678u;
  uint32_t iters = 0;
  while(!stop_flag) {
    tick = tick * 1103515245u + 12345u;
    iters++;
    if(iters == 201) raise(SIGUSR2);
  }
  H = fnv1a_u32(H, iters);
  H = fnv1a_u32(H, tick);

  uint32_t h_atomic = atomic_stress();
  H = fnv1a_u32(H, h_atomic);

  H = hash_mix_u32(H, buf, N);

  uint32_t s0 = buf[0];
  uint32_t s1 = buf[1];
  uint32_t s2 = buf[N-2];
  uint32_t s3 = buf[N-1];

  dup_free(buf);

  printf("checksum=%u\n", H);
  printf("sentinels=%u %u %u %u\n", s0, s1, s2, s3);
  printf("facts=%d s2d=%d diag=%d->%d got_signal=%d iters=%u\n",
         f10, s2d, d1, d2, (int)got_signal, iters);

  return 0;
}
