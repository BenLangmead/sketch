// bcqf_t::add_mt: the two-choice path must not take the same superblock
// lock twice when both candidate blocks share a superblock (it used to
// spin forever). A watchdog alarm turns a hang into a test failure.
#include <mutex>
#include "cqf.h"
#include <csignal>
#include <cstdio>
#include <random>
#include <thread>
#include <unistd.h>
#include <unordered_map>
using namespace sketch;

static void on_alarm(int) {
    const char msg[] = "regress_bcqf_addmt_lock: FAIL (add_mt hung: self-deadlock)\n";
    if(write(2, msg, sizeof(msg) - 1) < 0) {}
    _exit(1);
}

int main() {
    std::signal(SIGALRM, on_alarm);
    alarm(60);
    int failures = 0;
    // q = 10: 32 blocks, one superblock, so every two-choice insert has
    // both candidates in the same superblock
    {
        cqf::bcqf_t<32, 16> f(10);
        std::mt19937_64 r(1);
        std::unordered_map<uint64_t, uint64_t> ref;
        for(int i = 0; i < 700; ++i) {
            const uint64_t k = r() & 0xFFFFFFFFULL;
            f.add_mt(k); ++ref[k];
        }
        for(const auto &kv: ref) if(f.count(kv.first) != kv.second) ++failures;
        std::fprintf(stderr, "single thread q=10: %zu keys, %d count mismatches\n", ref.size(), failures);
    }
    // several threads over several superblocks, with growth
    {
        cqf::bcqf_t<32, 16> f(12);
        const unsigned nt = 4, per = 20000;
        std::vector<std::thread> th;
        for(unsigned t = 0; t < nt; ++t)
            th.emplace_back([&f, t]() {
                std::mt19937_64 r(100 + t);
                for(unsigned i = 0; i < per; ++i) f.add_mt(r() & 0xFFFFFFFFULL);
            });
        for(auto &x: th) x.join();
        std::unordered_map<uint64_t, uint64_t> ref;
        for(unsigned t = 0; t < nt; ++t) {
            std::mt19937_64 r(100 + t);
            for(unsigned i = 0; i < per; ++i) ++ref[r() & 0xFFFFFFFFULL];
        }
        int bad = 0;
        for(const auto &kv: ref) if(f.count(kv.first) != kv.second) ++bad;
        std::fprintf(stderr, "%u threads q=12 (grown to %u): %zu keys, %d count mismatches\n", nt, unsigned(f.q()), ref.size(), bad);
        failures += bad;
        try { f.structure_check(); } catch(const std::exception &e) { std::fprintf(stderr, "structure_check: %s\n", e.what()); ++failures; }
    }
    std::fprintf(stderr, "regress_bcqf_addmt_lock: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
