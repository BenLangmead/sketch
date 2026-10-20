// HyperMinHash merge and union_size with 64-bit registers on builds without
// AVX-512. The 64-bit lane max used there (vec.h DECMAX64) wrote lanes
// through a uint64_t pointer into a vector local, which GCC -O2 optimizes
// away under strict aliasing, so max(a, b) returned a: (a + b) and
// a.union_size(b) both gave a's cardinality. Checks the primitive itself
// (with unsigned values above 2^63), the merged registers for every
// register width, the merged and union cardinalities, and that
// union_sum_counts matches the histogram of the merged sketch.
#include "hmh.h"
#include <cmath>
#include <cstdio>
using namespace sketch;
static int failures = 0;

static void check_primitive() {
#ifndef VEC_DISABLED__
    using S = vec::SIMDTypes<uint64_t>;
    constexpr size_t n = S::COUNT;
    alignas(64) uint64_t a[n], b[n], mx[n], mn[n];
    for(size_t i = 0; i < n; ++i) {
        a[i] = (i & 1) ? (uint64_t(1) << 63) + i: i * 3;
        b[i] = (i & 1) ? i: (uint64_t(1) << 63) + 2 * i + 1;
    }
    S::storeu((S::Type *)mx, S::max(S::loadu((const S::Type *)a), S::loadu((const S::Type *)b)));
    S::storeu((S::Type *)mn, S::min(S::loadu((const S::Type *)a), S::loadu((const S::Type *)b)));
    int bad = 0;
    for(size_t i = 0; i < n; ++i) bad += mx[i] != std::max(a[i], b[i]) || mn[i] != std::min(a[i], b[i]);
    std::fprintf(stderr, "SIMDTypes<uint64_t>::max/min over %zu lanes: %d wrong lanes\n", n, bad);
    failures += bad != 0;
#endif
}

template<typename IT>
static void run(unsigned rs) {
    hmh::HyperMinHasher<> a(12, rs), b(12, rs);
    for(uint64_t i = 0; i < 300000; ++i) a.addh(i);
    for(uint64_t i = 200000; i < 600000; ++i) b.addh(i);
    auto m = a + b;
    size_t badreg = 0;
    for(size_t i = 0; i < (size_t(1) << 12); ++i)
        badreg += m.template access<IT>(i) != std::max(a.template access<IT>(i), b.template access<IT>(i));
    const double card = m.cardinality_estimate(), us = a.union_size(b);
    auto u = a.template union_sum_counts<IT>(b);
    auto s = m.template sum_counts<IT>();
    int badbin = 0;
    for(int i = 0; i < 64; ++i) badbin += u[i] != s[i];
    // relative error of an m=4096 sketch is about 1.6%; allow 5%
    const bool ok = badreg == 0 && badbin == 0 && std::abs(card - 6e5) < 3e4 && std::abs(us - 6e5) < 3e4;
    std::fprintf(stderr, "rsize=%2u: %zu wrong merged registers, merged card %.0f, union_size %.0f (true 600000), %d histogram bins differ %s\n",
        rs, badreg, card, us, badbin, ok ? "ok": "FAIL");
    failures += !ok;
}

int main() {
    check_primitive();
    run<uint8_t>(8);
    run<uint16_t>(16);
    run<uint32_t>(32);
    run<uint64_t>(64);
    std::fprintf(stderr, "regress_hmh64_merge: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
