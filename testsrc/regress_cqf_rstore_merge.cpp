// Truncated-remainder (RSTORE) mode: merging filters at the same q but
// with different stored widths (one grew to that q, one was built there)
// must not create false negatives. The merge used to re-embed the
// narrower side's remainders with a zero low bit in the wider store.
#include <mutex>
#include "cqf.h"
#include <cstdio>
#include <random>
using namespace sketch;
using F = cqf::cqfbase_t<40, 8, uint64_t, hash::WangHash, 10>;
static int failures = 0;
static size_t fn(const F &f, const std::vector<uint64_t> &ks) {size_t n = 0; for(auto k: ks) n += !f.contains(k); return n;}
static void check(const F &m, const std::vector<uint64_t> &ak, const std::vector<uint64_t> &bk, const char *tag) {
    const size_t fa = fn(m, ak), fb = fn(m, bk);
    bool sc = true;
    try { m.structure_check(); } catch(const std::exception &e) { sc = false; std::fprintf(stderr, "  structure_check: %s\n", e.what()); }
    std::fprintf(stderr, "%-8s false negatives: %zu / %zu of a's keys, %zu / %zu of b's keys\n", tag, fa, ak.size(), fb, bk.size());
    failures += fa != 0 || fb != 0 || !sc;
}
int main() {
    F a(8), b(9); // a grows 8 -> 9 (stored width 10 -> 9); b is built at 9 (stored width 10)
    std::mt19937_64 r(5);
    std::vector<uint64_t> ak, bk;
    while(a.q() < 9) { const uint64_t k = r(); a.add(k); ak.push_back(k); }
    for(int i = 0; i < 300; ++i) { const uint64_t k = r(); b.add(k); bk.push_back(k); }
    if(a.q() != b.q()) { std::fprintf(stderr, "setup: q mismatch\n"); return 2; }
    { F m(b); m += a; check(m, ak, bk, "b += a:"); }
    { F m(a); m += b; check(m, ak, bk, "a += b:"); }
    { F m = a + b; check(m, ak, bk, "a + b:"); }
    std::fprintf(stderr, "regress_cqf_rstore_merge: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
