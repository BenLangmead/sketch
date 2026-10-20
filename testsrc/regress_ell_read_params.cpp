// exaloglog: a sketch loaded from a file must behave exactly like the
// sketch that was written, including for later add() calls. read() used
// to rebuild t, d, p and the register width but keep the index and
// update-value masks of the constructor's (t=2, d=20, p=2) defaults, so
// add() after loading touched only registers 0..3.
#include "exaloglog.h"
#include <cstdio>
#include <unistd.h>
using namespace sketch;
static int failures = 0;
static void run(uint8_t t, uint8_t d, uint32_t p, uint64_t pre) {
    const std::string path = "regress_ell_read_params." + std::to_string(getpid()) + ".gz";
    ell::exaloglog_t fresh(t, d, p);
    for(uint64_t i = 0; i < pre; ++i) fresh.addh(i * 31 + 7);
    fresh.write(path);
    ell::exaloglog_t by_path(path);
    gzFile fp = gzopen(path.data(), "rb");
    ell::exaloglog_t by_gz(fp);
    gzclose(fp);
    ell::exaloglog_t reread(3, 10, 9); // different parameters, then read() over them
    reread.read(path);
    std::remove(path.data());
    for(uint64_t i = 0; i < 200000; ++i) {
        const uint64_t v = i * 0x9E3779B97F4A7C15ULL;
        fresh.addh(v); by_path.addh(v); by_gz.addh(v); reread.addh(v);
    }
    const bool ok = fresh == by_path && fresh == by_gz && fresh == reread
                 && fresh.estimate() == by_path.estimate() && fresh.estimate() == reread.estimate();
    std::fprintf(stderr, "t=%u d=%u p=%u pre=%llu: fresh %.0f, path ctor %.0f, gz ctor %.0f, read() %.0f %s\n",
        unsigned(t), unsigned(d), unsigned(p), (unsigned long long)pre,
        fresh.estimate(), by_path.estimate(), by_gz.estimate(), reread.estimate(), ok ? "ok": "MISMATCH");
    failures += !ok;
}
int main() {
    run(2, 20, 12, 0);
    run(2, 20, 12, 5000);
    run(1, 9, 10, 0);
    run(0, 2, 14, 1000);
    run(2, 20, 2, 0); // the constructor defaults: read() keeps the parameters
    std::fprintf(stderr, "regress_ell_read_params: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
