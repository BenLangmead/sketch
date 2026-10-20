// cqfbase_t::compose from a filter with wider counters must saturate new
// keys' counts at the destination's SAT. Masking instead wrapped 300 to 44
// and 256 to 0, and a zero count left a dead slot marked as a run.
#include <mutex>
#include "cqf.h"
#include <cstdio>
using namespace sketch;
int main() {
    int failures = 0;
    cqf::cqfbase_t<64, 16> src(8);
    const uint64_t k1 = 0x123456789abcdefULL, k2 = 0xfedcba9876543210ULL, k3 = 0x0f0f0f0f0f0f0f0fULL;
    for(int i = 0; i < 300; ++i) src.add(k1);
    for(int i = 0; i < 256; ++i) src.add(k2);
    for(int i = 0; i < 7; ++i) src.add(k3);
    cqf::cqfbase_t<64, 8> dst(8);
    dst.compose(src);
    const uint64_t c1 = dst.count(k1), c2 = dst.count(k2), c3 = dst.count(k3);
    std::fprintf(stderr, "dst counts %llu %llu %llu (want 255 255 7), size=%llu (want 3)\n",
        (unsigned long long)c1, (unsigned long long)c2, (unsigned long long)c3, (unsigned long long)dst.size());
    failures += c1 != 255 || c2 != 255 || c3 != 7 || dst.size() != 3;
    try { dst.structure_check(); } catch(const std::exception &e) { std::fprintf(stderr, "structure_check: %s\n", e.what()); ++failures; }
    // composing again saturates through the existing-key path
    dst.compose(src);
    failures += dst.count(k1) != 255 || dst.count(k3) != 14;
    std::fprintf(stderr, "regress_cqf_compose_saturate: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
