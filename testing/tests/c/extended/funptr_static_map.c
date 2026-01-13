// funptr_static_map.c
#include <stdio.h>
#include <stdint.h>

static int inc(int x) { return x + 1; }
static int mul3(int x) { return x * 3; }

static uint32_t checksum_ints(const int *a, int n) {
  uint32_t h = 2166136261u; // FNV-ish semplice
  for(int i = 0; i < n; i++) {
    h ^= (uint32_t)a[i];
    h *= 16777619u;
  }
  return h;
}

static void apply(int *a, int n, int (*op)(int)) {
  for(int i = 0; i < n; i++) {
    a[i] = op(a[i]); // <-- NOVITÀ: chiamata indiretta via function pointer
  }
}

int main(void) {
  const int N = 16;
  int a[N];

  // init deterministico
  for(int i = 0; i < N; i++) a[i] = i - 5;

  // scegliamo l'operazione in modo deterministico (niente input, niente random)
  int (*op)(int) = (N % 2 == 0) ? inc : mul3;

  apply(a, N, op);

  // output stabile: stampa checksum + 4 sentinelle
  uint32_t h = checksum_ints(a, N);
  printf("checksum=%u\n", h);
  printf("sentinels=%d %d %d %d\n", a[0], a[1], a[N-2], a[N-1]);

  return 0;
}
