#include <odl/gravity/coefficients.hpp>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <vector>

namespace odl::gravity {

namespace {

constexpr std::size_t kExpectedRecords = 2401333;   // GRAV-R-014, measured
constexpr int kMaxDegree = kEgm2008MaxDegree;

/// One field of one record.  The exponent marker is FORTRAN `D`
/// (GRAV-R-011): a reader that only accepts `E` reads nothing, and one that
/// stops at the `D` reads a mantissa and silently drops the exponent, which is
/// the quieter and far worse failure.
struct Cursor {
    const char* p;
    const char* end;

    void skip_space() noexcept {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    }
    [[nodiscard]] bool at_end() noexcept { skip_space(); return p >= end; }

    bool read_int(int& out) noexcept {
        skip_space();
        const char* s = p;
        bool neg = false;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); ++p; }
        long v = 0;
        const char* digits = p;
        while (p < end && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; }
        if (p == digits) { p = s; return false; }
        out = static_cast<int>(neg ? -v : v);
        return true;
    }

    bool read_double(double& out) noexcept {
        skip_space();
        char buf[64];
        std::size_t k = 0;
        const char* s = p;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && k + 1 < sizeof buf) {
            char ch = *p++;
            if (ch == 'D' || ch == 'd') ch = 'E';
            buf[k++] = ch;
        }
        if (k == 0) { p = s; return false; }
        buf[k] = '\0';
        char* stop = nullptr;
        errno = 0;
        out = std::strtod(buf, &stop);
        // The whole token must be consumed.  A partial parse is how a `D`
        // exponent turns into a mantissa with the exponent discarded.
        return stop == buf + k && errno != ERANGE;
    }
};

GravityError malformed(const std::string& path, std::size_t record, const std::string& what) {
    std::ostringstream m;
    m << "coefficient file is not shaped as GRAV-R-011 requires.\n"
         "  file     " << path << "\n"
         "  record   " << record << " (1-based)\n"
         "  fault    " << what << "\n"
         "  expected {n, m, Cbar, Sbar, sigmaC, sigmaS} as 2i5, 2d25.15, 2d20.10, with a\n"
         "           FORTRAN 'D' exponent marker, " << kExpectedRecords
      << " records, degrees 0 and 1 absent.";
    return GravityError{"GRAV-F-003", m.str()};
}

}  // namespace

double CoefficientSet::degree_amplitude(int n) const noexcept {
    if (n < 0 || n > max_degree_) return 0.0;
    double sum = 0.0;
    for (int m = 0; m <= n; ++m) {
        const double cc = c(n, m), ss = s(n, m);
        sum += cc * cc + ss * ss;
    }
    return std::sqrt(sum);
}

odl::Result<CoefficientSet, GravityError>
CoefficientSet::load_egm2008(const std::string& path, const std::string& cache_root) {
    namespace fs = std::filesystem;

    // GRAV-R-010 / GRAV-F-007.  Nothing enters this tree undeclared, and a
    // coefficient set is the largest single input in it.
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
    const fs::path root = fs::weakly_canonical(fs::path(cache_root), ec);
    const auto rel = canonical.lexically_relative(root);
    if (root.empty() || rel.empty() || *rel.begin() == "..") {
        std::ostringstream m;
        m << "coefficient file is outside the manifest cache and will not be read.\n"
             "  requested  " << canonical.string() << "\n"
             "  cache root " << root.string() << "\n"
             "  Every external input is declared with a URL and a SHA-256 and fetched into the\n"
             "  cache (plan rule R11). The hash is checked before this module sees a path, so a\n"
             "  path from anywhere else is a file nothing has verified.";
        return odl::err(GravityError{"GRAV-F-007", m.str()});
    }

    std::FILE* f = std::fopen(canonical.string().c_str(), "rb");
    if (f == nullptr) {
        std::ostringstream m;
        m << "cannot open the coefficient file " << canonical.string();
        return odl::err(GravityError{"GRAV-F-003", m.str()});
    }

    CoefficientSet out;
    const std::size_t entries = index(kMaxDegree, kMaxDegree) + 1;
    out.c_.assign(entries, 0.0);
    out.s_.assign(entries, 0.0);
    out.max_order_at_.assign(static_cast<std::size_t>(kMaxDegree) + 1, -1);
    out.path_ = canonical.string();
    out.tide_system_ = TideSystem::TideFree;   // GRAV-R-015, from EGM08-RM (1)

    std::vector<char> buf(1u << 22);
    std::string carry;
    std::size_t record = 0;
    int prev_n = -1, prev_m = -1;
    bool fault = false;
    std::string fault_text;

    auto consume = [&](const char* begin, const char* end) {
        Cursor cur{begin, end};
        while (!fault && !cur.at_end()) {
            const char* record_start = cur.p;
            int n = 0, m = 0;
            double cv = 0.0, sv = 0.0, sc = 0.0, ss = 0.0;
            if (!cur.read_int(n) || !cur.read_int(m) || !cur.read_double(cv)
                || !cur.read_double(sv) || !cur.read_double(sc) || !cur.read_double(ss)) {
                cur.p = record_start;   // incomplete tail; keep it for the next block
                return static_cast<std::size_t>(end - record_start);
            }
            ++record;
            if (n < 0 || n > kMaxDegree || m < 0 || m > n) {
                fault = true;
                std::ostringstream t;
                t << "degree/order (" << n << ", " << m << ") is outside 0 <= m <= n <= "
                  << kMaxDegree;
                fault_text = t.str();
                return static_cast<std::size_t>(0);
            }
            // GRAV-R-012.  Degree 1 must vanish for a geocentric origin; a file
            // that carries one is referred to some other origin and using it
            // would displace every result by the offset it encodes.
            if (n == 1 && (cv != 0.0 || sv != 0.0)) {
                fault = true;
                std::ostringstream t;
                t << "degree-1 coefficient (1, " << m << ") is non-zero: C = " << cv
                  << ", S = " << sv
                  << ". Degree 1 vanishes when the origin is the centre of mass; a non-zero "
                     "value means these coefficients are referred to some other origin.";
                fault_text = t.str();
                return static_cast<std::size_t>(0);
            }
            if (record == 1 && !(n == 2 && m == 0)) {
                fault = true;
                std::ostringstream t;
                t << "first record is (" << n << ", " << m
                  << "); EGM2008 omits degrees 0 and 1 entirely and starts at (2, 0)";
                fault_text = t.str();
                return static_cast<std::size_t>(0);
            }
            if (prev_n >= 0) {
                const bool next_in_column = (n == prev_n && m == prev_m + 1);
                const bool next_degree = (n == prev_n + 1 && m == 0);
                if (!next_in_column && !next_degree) {
                    fault = true;
                    std::ostringstream t;
                    t << "records are out of sequence: (" << prev_n << ", " << prev_m
                      << ") followed by (" << n << ", " << m << ")";
                    fault_text = t.str();
                    return static_cast<std::size_t>(0);
                }
            }
            prev_n = n;
            prev_m = m;
            const std::size_t i = index(n, m);
            out.c_[i] = cv;
            out.s_[i] = sv;
            if (cv != 0.0 || sv != 0.0) {
                out.max_order_at_[static_cast<std::size_t>(n)] =
                    std::max(out.max_order_at_[static_cast<std::size_t>(n)], m);
                out.max_non_zero_order_ = std::max(out.max_non_zero_order_, m);
            }
        }
        return static_cast<std::size_t>(0);
    };

    while (!fault) {
        const std::size_t got = std::fread(buf.data() + carry.size(), 1,
                                           buf.size() - carry.size(), f);
        if (carry.size() > 0) std::memcpy(buf.data(), carry.data(), carry.size());
        const std::size_t have = carry.size() + got;
        if (have == 0) break;
        const std::size_t left = consume(buf.data(), buf.data() + have);
        carry.assign(buf.data() + have - left, left);
        if (got == 0) break;
    }
    const bool read_error = std::ferror(f) != 0;
    std::fclose(f);

    if (fault) return odl::err(malformed(canonical.string(), record, fault_text));
    if (read_error) return odl::err(malformed(canonical.string(), record, "read error"));

    if (record != kExpectedRecords) {
        std::ostringstream t;
        t << "record count is " << record << ", expected " << kExpectedRecords
          << ". That is the full triangle to (2190, 2190) less the three records of degrees 0 "
             "and 1, which the file omits. A truncated download parses cleanly up to the point "
             "it stops, so the count is the only thing that catches it.";
        return odl::err(malformed(canonical.string(), record, t.str()));
    }

    // DEGREE 0 IS 1, NOT 0, and the specification said 0 until implementation
    // reached this line.  TN36-6 (6.1) sums from n = 0 with Cbar00 carrying the
    // two-body term, so V = (GM/r)[1 + ...] falls out of the same sum as
    // everything else; the EGM2008 README writes the equivalent form with the 1
    // outside the sum and the sum starting at n = 2.  Setting it here means the
    // synthesis needs no special case for n = 0, GRAV-R-027's exact GM/r^2 at
    // degree 0 is a consequence rather than a separate code path, and
    // degree_amplitude(0) is 1, which is what it should be.  Degree 1 is zero,
    // and stays zero, for the quite different reason that the origin is the
    // centre of mass.
    out.c_[index(0, 0)] = 1.0;
    out.max_order_at_[0] = 0;

    out.records_ = record;
    out.max_degree_ = prev_n;
    return out;
}

}  // namespace odl::gravity
