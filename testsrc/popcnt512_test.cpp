// Checks popcnt512 (per-64-bit-lane population count) against __builtin_popcountll on random,
// all-zero and all-one lanes. Skipped in builds without AVX-512.
#include "sketch/integral.h"
#include <cstdio>
#include <random>
#ifdef __AVX512F__
int main() {
    std::mt19937_64 rng(7);
    for(int it = 0; it < 100000; ++it) {
        alignas(64) uint64_t in[8], out[8];
        for(auto &x: in) { x = rng(); if(it % 7 == 0) x = 0; if(it % 11 == 0) x = ~uint64_t(0); }
        _mm512_store_si512((__m512i *)out, popcnt512(_mm512_load_si512((const __m512i *)in)));
        for(int i = 0; i < 8; ++i) if(out[i] != uint64_t(__builtin_popcountll(in[i]))) { std::printf("FAIL it %d lane %d\n", it, i); return 1; }
    }
    std::puts("PASS popcnt512");
}
#else
int main() {std::puts("SKIP popcnt512 (no AVX-512)");}
#endif
