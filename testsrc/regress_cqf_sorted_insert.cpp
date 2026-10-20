// cqfbase_t bulk insert: insert_buffered, insert_sorted and
// insert_sorted_counts must give the same counts as add() for every
// KEY_BITS, input order and starting table. With KEY_BITS < 64, sorting by
// the full 64-bit hash is not (home, remainder) order, and the append
// cursor used to place a smaller home's run after a larger one's.
#include <mutex>
#include "cqf.h"
#include <algorithm>
#include <cstdio>
#include <random>
#include <unordered_map>
using namespace sketch;
static int failures = 0;

template<typename F>
static void verify(const F &f, const std::unordered_map<uint64_t, uint64_t> &ref, const char *tag) {
    size_t miss = 0;
    for(const auto &kv: ref) miss += f.count(kv.first) != kv.second;
    bool sc = true;
    try { f.structure_check(); } catch(const std::exception &e) { sc = false; std::fprintf(stderr, "  structure_check: %s\n", e.what()); }
    std::fprintf(stderr, "%-44s %zu / %zu keys with wrong count%s\n", tag, miss, ref.size(), sc ? "": ", structure_check failed");
    failures += miss != 0 || !sc;
}

template<size_t KB>
static void run(const char *name) {
    using F = cqf::cqfbase_t<KB, 8>;
    char tag[128];
    std::vector<uint64_t> vals(2000);
    for(size_t i = 0; i < vals.size(); ++i) vals[i] = i * 7919 + 13;
    std::unordered_map<uint64_t, uint64_t> ref;
    {
        F f(12);
        for(auto v: vals) ++ref[f.hash(v)];
        f.insert_buffered(vals.begin(), vals.end());
        std::snprintf(tag, sizeof(tag), "%s insert_buffered:", name); verify(f, ref, tag);
    }
    {
        // insert_buffered with a small threshold: several flushes into a
        // nonempty table, and a grow from q=8
        F f(8);
        f.insert_buffered(vals.begin(), vals.end(), 300);
        std::snprintf(tag, sizeof(tag), "%s insert_buffered(300) grow:", name); verify(f, ref, tag);
    }
    {
        // insert_sorted with full hashes sorted ascending, as its doc says
        F f(12);
        std::vector<uint64_t> hs;
        for(auto v: vals) hs.push_back(f.hash(v));
        std::sort(hs.begin(), hs.end());
        f.insert_sorted(hs.begin(), hs.end());
        std::snprintf(tag, sizeof(tag), "%s insert_sorted(full-hash order):", name); verify(f, ref, tag);
    }
    {
        // insert_sorted_counts in random order on top of add()-inserted keys
        F f(12);
        std::unordered_map<uint64_t, uint64_t> ref2;
        std::mt19937_64 r(7);
        for(int i = 0; i < 500; ++i) { const uint64_t h = f.hash(r()); f.add(h); ++ref2[h]; }
        std::vector<std::pair<uint64_t, uint64_t>> kc;
        for(auto v: vals) kc.emplace_back(f.hash(v), 3);
        std::shuffle(kc.begin(), kc.end(), r);
        for(const auto &p: kc) ref2[p.first] += 3;
        f.insert_sorted_counts(kc.begin(), kc.end());
        std::snprintf(tag, sizeof(tag), "%s insert_sorted_counts(random, nonempty):", name); verify(f, ref2, tag);
    }
}

int main() {
    run<40>("KEY_BITS=40");
    run<48>("KEY_BITS=48");
    run<64>("KEY_BITS=64");
    std::fprintf(stderr, "regress_cqf_sorted_insert: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
