// cqfbase_t random-order add() must cost about the same per insert at any
// fill level. Packing every run to the left of the table left holes only
// at the frontier, so each insert below it memmoved the rest of the live
// region and a build was quadratic (3.1 us per insert at 50k keys, 33.6 us
// at 400k, on a q=20 table). Home-anchored placement keeps holes spread
// out. The second half checks correctness of that layout: random adds and
// erases against a reference map through grows, runs that spill into the
// tail past nslots, merges and compose.
#include <mutex>
#include "cqf.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <unordered_map>
using namespace sketch;
static int failures = 0;

template<typename F>
static void verify(const F &f, const std::unordered_map<uint64_t, uint64_t> &ref, const char *tag) {
    size_t bad = 0;
    for(const auto &kv: ref) bad += f.count(kv.first) != kv.second;
    bool sc = true;
    try { f.structure_check(); } catch(const std::exception &e) { sc = false; std::fprintf(stderr, "  structure_check: %s\n", e.what()); }
    uint64_t total = 0;
    for(const auto &kv: ref) total += kv.second != 0;
    const bool sz = f.size() == total;
    std::fprintf(stderr, "%-34s %zu / %zu wrong counts, size %llu (want %llu)%s\n", tag, bad, ref.size(),
        (unsigned long long)f.size(), (unsigned long long)total, sc ? "": ", structure_check failed");
    failures += bad != 0 || !sc || !sz;
}

static double per_insert_ns(unsigned n) {
    double best = 1e300;
    for(int rep = 0; rep < 3; ++rep) {
        cqf::cqf64_16 f(20);
        std::mt19937_64 r(n + rep);
        const auto t0 = std::chrono::steady_clock::now();
        for(unsigned i = 0; i < n; ++i) f.add(r());
        best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / n * 1e9);
    }
    return best;
}

int main() {
    // scaling: per-insert cost at 400k keys vs 50k keys in a q=20 table
    // (load 0.05 vs 0.38); the left-packed layout gave a ratio near 11
    {
        const double a = per_insert_ns(50000), b = per_insert_ns(400000);
        const double ratio = b / a;
        std::fprintf(stderr, "random add() q=20: %.0f ns/insert at 50k, %.0f ns/insert at 400k, ratio %.2f (limit 4)\n", a, b, ratio);
        failures += ratio > 4.;
    }
    // random adds and erases with growth from q=8
    {
        cqf::cqf64_8 f(8);
        std::unordered_map<uint64_t, uint64_t> ref;
        std::mt19937_64 r(11);
        std::vector<uint64_t> keys;
        for(int i = 0; i < 6000; ++i) keys.push_back(r());
        for(int step = 0; step < 40000; ++step) {
            const uint64_t k = keys[r() % keys.size()];
            if(r() % 4 == 0) {
                const bool had = ref.count(k) && ref[k] > 0;
                if(f.dec(k) != had) { ++failures; std::fprintf(stderr, "dec(%llu) disagreed with reference\n", (unsigned long long)k); break; }
                if(had && --ref[k] == 0) ref.erase(k);
            } else {
                f.add(k);
                if(ref[k] < 255) ++ref[k];
            }
        }
        verify(f, ref, "add/dec mix with grows:");
    }
    // a run at the top home spills past nslots into the tail; merge and
    // compose must visit those slots
    {
        cqf::cqf64_8 a(8), b(8);
        std::unordered_map<uint64_t, uint64_t> ref;
        for(uint64_t i = 0; i < 24; ++i) { const uint64_t k = (uint64_t(255) << 56) | (i * 0x10001ULL + 1); a.add(k); ++ref[k]; }
        for(uint64_t i = 0; i < 12; ++i) { const uint64_t k = (uint64_t(254) << 56) | (i * 0x10003ULL + 5); a.add(k); ++ref[k]; }
        // xnslots = 256 + 2 * 16 + 8 = 296: home 254's run takes 254..265,
        // home 255's takes 266..289, all at q = 8
        verify(a, ref, "top-home run spilling into tail:");
        if(a.q() != 8) { std::fprintf(stderr, "setup: expected the filter to stay at q=8\n"); ++failures; }
        for(uint64_t i = 0; i < 40; ++i) { const uint64_t k = (uint64_t(i % 7) << 56) | (i * 0x777ULL + 3); b.add(k); ++ref[k]; }
        {
            cqf::cqf64_8 m(b); m += a; // a is the smaller side
            verify(m, ref, "merge of the spilled filter:");
        }
        cqf::cqfbase_t<64, 4> c(8);
        c.compose(a);
        size_t bad = 0;
        for(uint64_t i = 0; i < 24; ++i) bad += c.count((uint64_t(255) << 56) | (i * 0x10001ULL + 1)) != 1;
        std::fprintf(stderr, "%-34s %zu / 24 wrong counts\n", "compose of the spilled filter:", bad);
        failures += bad != 0;
    }
    std::fprintf(stderr, "regress_cqf_random_insert: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
