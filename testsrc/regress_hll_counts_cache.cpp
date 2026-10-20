// hll_t caches its register histogram (counts_) for the joint estimators.
// A sketch obtained by copy construction, read() from a gz file or a raw
// fd, or parsum() must give the same ertl_joint result as the original;
// these paths used to set value_ without counts_, so ccounts() returned an
// all-zero or stale histogram and ertl_joint gave -inf / inf.
#include "hll.h"
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
using namespace sketch;
static int failures = 0;
static std::array<double, 3> ref;
static void check(const char *tag, std::array<double, 3> v) {
    bool ok = true;
    for(int i = 0; i < 3; ++i) ok = ok && std::isfinite(v[i]) && v[i] == ref[i];
    std::fprintf(stderr, "%-24s A-B=%.1f B-A=%.1f A&B=%.1f %s\n", tag, v[0], v[1], v[2], ok ? "ok": "MISMATCH");
    failures += !ok;
}
int main() {
    hll::hll_t a(12), b(12);
    a.set_jestim(hll::ERTL_JOINT_MLE); b.set_jestim(hll::ERTL_JOINT_MLE);
    for(uint64_t i = 0; i < 100000; ++i) a.addh(i);
    for(uint64_t i = 50000; i < 150000; ++i) b.addh(i);
    a.sum(); b.sum();
    ref = hll::ertl_joint(a, b);
    check("original", ref);
    { hll::hll_t c(a); check("copy-constructed", hll::ertl_joint(c, b)); }
    { hll::hll_t d(12); d = a; check("copy-assigned", hll::ertl_joint(d, b)); }
    // copy constructed from a sketch whose histogram was never computed
    // into the copy, after the source was summed for a different content
    { hll::hll_t old(12); old.set_jestim(hll::ERTL_JOINT_MLE); old.sum(); old = a; hll::hll_t c(old); check("copy of assigned", hll::ertl_joint(c, b)); }
    const std::string base = "regress_hll_counts_cache." + std::to_string(getpid());
    {
        const std::string p = base + ".gz";
        a.write(p);
        hll::hll_t e(12); e.read(p);
        check("read(gz path)", hll::ertl_joint(e, b));
        hll::hll_t f(p);
        check("path constructor", hll::ertl_joint(f, b));
        std::remove(p.data());
    }
    {
        const std::string p = base + ".raw";
        a.write(p.data(), false);
        hll::hll_t e(12);
        const int fd = open(p.data(), O_RDONLY);
        e.read(fd);
        close(fd);
        check("read(fd)", hll::ertl_joint(e, b));
        std::remove(p.data());
    }
    {
        // a sketch that summed one content, then had its registers replaced
        // by read(): the histogram must follow the new registers
        hll::hll_t g(12); g.set_jestim(hll::ERTL_JOINT_MLE);
        for(uint64_t i = 900000; i < 905000; ++i) g.addh(i);
        g.sum();
        const std::string p = base + ".2.gz";
        a.write(p);
        g.read(p);
        std::remove(p.data());
        check("read() over summed", hll::ertl_joint(g, b));
    }
    {
        hll::hll_t h(12); h.set_jestim(hll::ERTL_JOINT_MLE);
        for(uint64_t i = 0; i < 100000; ++i) h.addh(i);
        h.parsum(2);
        check("parsum()", hll::ertl_joint(h, b));
    }
    std::fprintf(stderr, "regress_hll_counts_cache: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
