// cqfbase_t::read must reject a file written with a different VAL_BITS
// and a header with an out-of-range q, instead of parsing the payload
// with the wrong layout.
#include <mutex>
#include "cqf.h"
#include <cstdio>
#include <cstring>
#include <sys/resource.h>
#include <unistd.h>
using namespace sketch;
int main() {
    // a header q that is not rejected makes init() allocate 2^q slots; cap
    // the address space so that turns into bad_alloc instead of paging
    const rlimit lim{rlim_t(1) << 30, rlim_t(1) << 30};
    setrlimit(RLIMIT_AS, &lim);
    int failures = 0;
    const std::string path = "regress_cqf_read_checks." + std::to_string(getpid()) + ".gz";
    cqf::cqf64_16 a(8);
    for(uint64_t i = 1; i < 100; ++i) for(uint64_t j = 0; j < i; ++j) a.add(i * 0x9E3779B97F4A7C15ULL);
    a.write(path);
    // round trip with the same type still works
    {
        cqf::cqf64_16 b(cqf::cqf64_16::from_path, path);
        const bool ok = b.count(50 * 0x9E3779B97F4A7C15ULL) == 50 && b.size() == a.size();
        std::fprintf(stderr, "same-type round trip: %s\n", ok ? "ok": "wrong");
        failures += !ok;
    }
    // VAL_BITS 16 file read as VAL_BITS 8
    try {
        cqf::cqf64_8 b(cqf::cqf64_8::from_path, path);
        std::fprintf(stderr, "VAL_BITS=16 file read as VAL_BITS=8: no throw (count(k50)=%llu)\n", (unsigned long long)b.count(50 * 0x9E3779B97F4A7C15ULL));
        ++failures;
    } catch(const std::exception &e) { std::fprintf(stderr, "VAL_BITS mismatch throws: %s\n", e.what()); }
    // out-of-range q in the header
    for(uint32_t badq: {1u, 41u, 63u, 200u}) {
        gzFile fp = gzopen(path.data(), "wb");
        const uint32_t header[3]{0x43514634, badq, 16};
        const uint64_t zero[2]{0, 0};
        gzwrite(fp, header, sizeof(header)); gzwrite(fp, zero, sizeof(zero));
        gzclose(fp);
        try {
            cqf::cqf64_16 b(cqf::cqf64_16::from_path, path);
            std::fprintf(stderr, "q=%u header: no throw\n", badq);
            ++failures;
        } catch(const std::exception &e) {
            // only the header check counts: a later allocation or short-read
            // failure means the bad q was accepted
            const bool ok = std::strstr(e.what(), "invalid q") != nullptr;
            std::fprintf(stderr, "q=%u header throws (%s): %s\n", badq, ok ? "header check": "not the header check", e.what());
            failures += !ok;
        }
    }
    std::remove(path.data());
    std::fprintf(stderr, "regress_cqf_read_checks: %s\n", failures ? "FAIL": "PASS");
    return failures != 0;
}
