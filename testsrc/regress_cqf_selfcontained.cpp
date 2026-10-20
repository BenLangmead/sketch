// cqf.h must compile as the first and only sketch include: add_mt's grow
// path uses std::unique_lock, which needs <mutex>. This test fails at
// compile time when the header is not self-contained.
#include "cqf.h"
#include <cstdio>

int main() {
    sketch::cqf::bcqf_t<32, 16> f(10);
    for(uint64_t i = 0; i < 100; ++i) f.add_mt(i * 0x9E3779B97F4A7C15ULL & 0xFFFFFFFFULL);
    const bool ok = f.size() > 0;
    std::fprintf(stderr, "regress_cqf_selfcontained: %s\n", ok ? "PASS": "FAIL");
    return !ok;
}
