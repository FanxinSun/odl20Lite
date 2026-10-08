// tests/devtools/measmod_fd_sizing_tests.cpp -- the finite-difference gate's sizing, SPEC-measmod.md 6.2 (plan L0 step 8, group C8): every function against values computed apart, the Python's own recorded output for
// `--scan --check` byte for byte, the other three modes derived from that record by the Python's own flags, the diurnal section against what was registered for it, every `--check` comparison shown firing at its tolerance,
// the refusals, the command line.
// (ctests `measmod_fd_sizing.behaviour` and `measmod_fd_sizing.real_tree`; the old ctest name measmod.fd_sizing_reproduces runs the tool itself.)
//
// THE ORACLES, none of them an output of the port:
//   * R1, the output of the PYTHON's `--scan --check` as L6's Round 9 report recorded it (tests/devtools/measmod_fd_sizing_recorded.hpp, 106 lines): the port's lines 1-105 and its last line equal it, and the other modes are
//     R1 with the lines the Python's flags leave out (below).
//   * the 12 lines of the diurnal section registered before any C++ existed (the table rows are the closed forms of the specification by bc at 50 digits), and for the 18 digits of it that no record has, closed forms and the
//     Python's own frozen literals (below).
//   * GNU bc 1.07.1 at 60 digits from the FORMULAS of the tool's documentation and of SPEC-measmod 6.2 (C8_registered/fd_values.bc, atm_worked.bc, worked_floor.bc in this group's report files): the sizing, the stencil
//     function F_geo, the chain's bound, eps, F_pure.  The published IERS FCUL_A and ERFA eraRefco cases.  Hand derivations for the series (dyadic numbers) and for a few lines of sight.
//   * the Python source (tools/measmod_fd_sizing.py), read for which flag prints which line and for the tolerance of each comparison.

#include <catch2/catch_test_macros.hpp>

#include "measmod_fd_sizing_recorded.hpp"
#include "measmod_synthetic_files.hpp"
#include "throwing_stream.hpp"

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_fd_sizing.hpp"
#include "measmod_reference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <numbers>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace fd = odl::tools::measmod_fd_sizing;
namespace mr = odl::tools::measmod_reference;
using namespace odl::devkit;
using namespace odl::devtools_testing;

namespace {

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const fd::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = fd::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_real(const std::vector<std::string>& args) { return run_with(fd::default_settings(), args); }

// the four modes on the real tree, each run once (the scan modes take a second)
const Result& real_plain() {
    static const Result r = run_real({});
    return r;
}
const Result& real_check() {
    static const Result r = run_real({"--check"});
    return r;
}
const Result& real_scan() {
    static const Result r = run_real({"--scan"});
    return r;
}
const Result& real_scan_check() {
    static const Result r = run_real({"--scan", "--check"});
    return r;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

std::vector<std::string> slice(const std::vector<std::string>& v, std::size_t first, std::size_t count) {
    std::vector<std::string> out;
    for (std::size_t i = first; i < first + count && i < v.size(); ++i) out.push_back(v[i]);
    return out;
}

bool ends_with(const std::string& s, const std::string& suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }
bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// the double nearest a decimal given in text
double nearest(const char* text) { return std::strtod(text, nullptr); }

bool close(double got, double want, double relative) { return std::fabs(got - want) <= relative * std::fabs(want); }

// |got - want| <= tolerance, with the numbers shown when it is not
bool within(double got, double want, double tolerance, const char* what) {
    const bool ok = std::fabs(got - want) <= tolerance;
    if (!ok) {
        std::ostringstream message;
        message << std::setprecision(17) << what << ": got " << got << ", want " << want << ", difference " << got - want << ", allowed " << tolerance;
        UNSCOPED_INFO(message.str());
    }
    return ok;
}

// the numbers of a piece of text, as the Python printed them: digits with an optional point and exponent, a sign directly before them counting
std::vector<double> numbers_in(const std::string& text) {
    std::vector<double> out;
    std::size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        const bool digit = ch >= '0' && ch <= '9';
        const bool signed_digit = (ch == '-' || ch == '+') && i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9';
        if (!digit && !signed_digit) {
            ++i;
            continue;
        }
        char* end = nullptr;
        out.push_back(std::strtod(text.c_str() + i, &end));
        i = static_cast<std::size_t>(end - text.c_str());
    }
    return out;
}

// the text between the first `open` and the next `close` after it (empty when either is missing)
std::string between(const std::string& s, const std::string& open, const std::string& close) {
    const std::size_t a = s.find(open);
    if (a == std::string::npos) return "";
    const std::size_t from = a + open.size();
    const std::size_t b = s.find(close, from);
    return b == std::string::npos ? "" : s.substr(from, b - from);
}

// the context the tool computes in
struct Sixty {
    LocalContext context{mr::kPrecision};
};

void expect_lines(const std::vector<std::string>& got, const std::vector<std::string>& want, const std::string& what) {
    INFO(what);
    CHECK(got.size() == want.size());
    const std::size_t n = std::min(got.size(), want.size());
    for (std::size_t i = 0; i < n; ++i) {
        INFO("line " << i + 1 << " of " << what);
        CHECK(got[i] == want[i]);
    }
}

// ---- the Python's recorded output, and what each flag prints ---------------------------------------------------------------------------------------------------------------------------------------------

const std::vector<std::string>& recorded() {
    static const std::vector<std::string> lines = lines_of(kRecordedScanCheck);
    return lines;
}

enum class Flag { Common, Check, Scan, ScanCheck };

// Which flag prints line `number` (1-based) of the recorded output: read from measmod_fd_sizing.py, see the registration C8-T (b) in this group's report files.
Flag flag_of(std::size_t number) {
    static const std::set<std::size_t> check = {8, 16, 24, 31, 32, 49, 50, 67, 68, 87, 88, 89, 90, 91, 92, 106};
    static const std::set<std::size_t> scan = {26, 34, 45, 46, 47, 48, 52, 63, 64, 65, 66, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105};
    if (number == 27) return Flag::ScanCheck;
    if (check.count(number) != 0) return Flag::Check;
    if (scan.count(number) != 0) return Flag::Scan;
    return Flag::Common;
}

// the lines of the recorded output (the last one, the verdict, left out) that a run with these flags prints
std::vector<std::string> derived_recorded(bool scan, bool check) {
    std::vector<std::string> out;
    for (std::size_t n = 1; n <= 105; ++n) {
        const Flag f = flag_of(n);
        const bool wanted = f == Flag::Common || (f == Flag::Check && check) || (f == Flag::Scan && scan) || (f == Flag::ScanCheck && scan && check);
        if (!wanted) continue;
        out.push_back(!scan && (n == 49 || n == 67) ? "    frozen F_trop in use: ok" : recorded()[n - 1]);   // (Python line 407: the wording without --scan)
    }
    return out;
}

// the twelve lines of the diurnal section registered before the C++ existed: the title, the geometry (7.2921150e-5 * 6378137 = 465.10108490 m/s, over c, by bc), nu = 3 (2^-52 + 2^-30 / 1.2e6) = 2.99444025e-15
// (by hand and by bc), F_extra, and the five rows of the table by bc at 50 digits from the closed forms
const std::vector<std::string>& diurnal_registered() {
    static const std::vector<std::string> lines = {
        "",
        "=== THE DIURNAL-ABERRATION CHECK (MEAS-A-098b; registered after the angle gate's one run, before this check has run) ===",
        "geometry: azimuth 90 deg, elevation 30 deg, rho 1.2e+06 m, the observer moving at 465.1011 m/s along the line of sight; beta = 1.551410e-06",
        "nu (this geometry) = 3 [ulp(az = 1.5708 rad) + ulp(target's largest component)/rho] = 2.9944e-15 rad   (the angle gate's nu_a, with ulp(2 pi), was 4.9928e-15)",
        "F_extra/rho^3 frozen = 0.0219  (1.05 x the fine scan's 2.0815e-02, rounded up)",
        "    h        F_pure(h; 30 deg)    F_a(h)               eps(h)",
        "        10   1.78202e-18    1.79469e-18    3.2936e-16",
        "        30   1.78216e-18    1.79483e-18    3.6904e-16",
        "       100   1.78265e-18    1.79532e-18    3.0221e-15",
        "       300   1.78405e-18    1.79673e-18    2.6961e-14",
        "      1000   1.78899e-18    1.80166e-18    3.0028e-13",
        "    B_pred = max eps = 3.0028e-13, window [3.003e-14, 3.003e-12]"};
    return lines;
}

const std::vector<std::string>& diurnal_check_lines() {
    static const std::vector<std::string> lines = {"the frozen nu of this geometry reproduced: ok",
                                                   "the frozen eps(h) and B_pred of this geometry reproduced: ok",
                                                   "the frozen F_extra is 1.05 x the fine scan, rounded up to three digits: ok",
                                                   "the frozen row differences (without A', A' reversed) reproduced: ok",
                                                   "the frozen predicted powers of the two controls (in eps, at the five sizes) reproduced: ok"};
    return lines;
}

// the {:.4e} numbers of a line, as the text the Python's f-string made of them (-?d.dddde+-dd)
std::vector<std::string> e4_texts_in(const std::string& line) {
    std::vector<std::string> out;
    const std::regex number("-?[0-9]\\.[0-9]{4}e[-+][0-9]{2}");
    for (std::sregex_iterator it(line.begin(), line.end(), number), end; it != end; ++it) out.push_back(it->str());
    return out;
}

// LINES 118-132 OF THE FULL OUTPUT HAVE NO RECORD, AND ARE PINNED BY CONSTRUCTION.  The Python's output was kept for the lines 1-105 and the last (recorded() above) and the diurnal section's table was registered before the C++
// existed (diurnal_registered()); the fifteen lines after the table -- the six lines of the two controls and the model's row (118-123, checked by check_diurnal_controls), the five check lines (124-128, diurnal_check_lines())
// and the four lines of the scan (129-132, check_diurnal_scan) -- were never kept anywhere.  What pins them, instead of a record (the maintainer's ruling on group C8, 2026-10-08: "no record; pinned by construction"):
// the Python's FROZEN LITERALS that its own checks compare with, CLOSED FORMS worked out by hand and by bc, the LAYOUT of the Python's f-strings (a regular expression for each line) and RANGES that the Python's comments and the
// specification give.  A defect that changes nothing of what these print, or a digit below the ones they pin, is a survivor of a stated class (PRT, DATA), not a gap -- until a consumer needs more digits than this.
// The six lines after the table: for each sign of the observer's motion (toward the target, away), the two controls' largest row differences and the model's row.  Their digits are recorded nowhere; what stands for them:
//   * the row differences are the Python's frozen literals 1.4928e-12 and 2.9857e-12 (rad/m), to the five digits printed, and in eps at the five sizes the frozen whole numbers 4533 4045 494 55 5 and 9065 8090 988 111 10, to 1;
//   * the model's row is the Jacobian of (azimuth, elevation) with respect to the target's position along the GCRS axes x = up, y = east, z = north, at azimuth 90 deg, elevation 30 deg, rho = 1.2e6 m: by hand,
//     d el / d up = cos(el) / (rho q), d el / d east = - sin(el) / (rho q), d az / d north = - 1 / (rho cos el), q = 1 + 4 A + 36 B (the refraction's dz_o/dz_v at z = 60 deg: 1 / (1 + A sec^2 z + 3 B tan^2 z sec^2 z)),
//     A = 2.484383e-04 and B = -3.123796e-07 (bc: 7.20979468726860e-07, -4.16257690349646e-07, -9.62250448649376e-07), the three others zero (the pole of the azimuth, a displacement along the line of sight, a
//     displacement normal to the plane): they print the round-off, 5e-39 and 6e-23 and 3e-23 on this machine, which no record fixes, so only |v| < 1e-20 is asked.
void check_diurnal_controls(const std::vector<std::string>& v) {
    REQUIRE(v.size() == 6);
    const std::array<double, 5> without = {4533.0, 4045.0, 494.0, 55.0, 5.0};
    const std::array<double, 5> reversed = {9065.0, 8090.0, 988.0, 111.0, 10.0};
    const std::array<const char*, 2> motion = {"toward", "away  "};
    for (std::size_t s = 0; s < 2; ++s) {
        for (std::size_t variant = 0; variant < 2; ++variant) {
            const std::string& line = v[3 * s + variant];
            const std::string prefix = std::string("    observer moving ") + motion[s] + " the target: row " + (variant == 0 ? "without  " : "reversed ") + ": max |wrong - right| = ";
            INFO(line);
            REQUIRE(starts_with(line, prefix));
            const std::vector<double> n = numbers_in(line.substr(prefix.size()));
            REQUIRE(n.size() == 8);   // the difference, its multiple of eps, the size it is at, and the five multiples
            CHECK(close(n[0], variant == 0 ? 1.4928e-12 : 2.9857e-12, 2e-4));
            // the frozen literals ARE the Python's output: the difference prints as exactly 1.4928e-12 (2.9857e-12), the multiple of eps as 4533 (9065), and the five multiples are the frozen whole numbers
            CHECK(starts_with(line.substr(prefix.size()), variant == 0 ? "1.4928e-12 rad/m = 4533 eps (at h = 10 m)  [per size: ['4533', '4045', '494', '55', '5']]" : "2.9857e-12 rad/m = 9065 eps (at h = 10 m)  [per size: ['9065', '8090', '988', '111', '10']]"));
            const std::array<double, 5>& powers = variant == 0 ? without : reversed;
            CHECK(n[1] == n[3]);   // the largest of the five is the first (the smallest size), as printed
            CHECK(n[2] == 10.0);
            for (std::size_t i = 0; i < 5; ++i) CHECK(std::fabs(n[3 + i] - powers[i]) <= 1.0);
            CHECK(ends_with(line, "]"));
            // the layout, as the Python's f-string writes it: the difference as {:.4e}, the multiple of eps as {:.0f} (a whole number), the size as {:.0f}, the five multiples as '{:.0f}' in a list
            CHECK(std::regex_match(line, std::regex("    observer moving (toward|away  ) the target: row (without  |reversed ): max \\|wrong - right\\| = [0-9]\\.[0-9]{4}e-12 rad/m = [0-9]+ eps \\(at h = 10 m\\)  \\[per size: \\['[0-9]+', '[0-9]+', '[0-9]+', '[0-9]+', '[0-9]+'\\]\\]")));
        }
        const std::string& row = v[3 * s + 2];
        const std::string prefix = "        the model's row (rad/m): x: (";
        INFO(row);
        REQUIRE(starts_with(row, prefix));
        CHECK(row.find("), y: (") != std::string::npos);
        CHECK(row.find("), z: (") != std::string::npos);
        CHECK(ends_with(row, ")"));
        const std::string number = "-?[0-9]\\.[0-9]{4}e[-+][0-9]{2}";   // {:.4e}
        CHECK(std::regex_match(row, std::regex("        the model's row \\(rad/m\\): x: \\(" + number + ", " + number + "\\), y: \\(" + number + ", " + number + "\\), z: \\(" + number + ", " + number + "\\)")));
        const std::vector<double> n = numbers_in(row.substr(prefix.size()));
        REQUIRE(n.size() == 6);
        CHECK(std::fabs(n[0]) < 1e-20);                                                   // d azimuth / d up
        CHECK(close(n[1], nearest(".000000720979468726860356160851271794590811877667367887611999"), 2e-5));    // d elevation / d up
        CHECK(std::fabs(n[2]) < 1e-20);                                                   // d azimuth / d east
        CHECK(close(n[3], nearest("-.000000416257690349646197107634108667142770552877973603996935"), 2e-5));   // d elevation / d east
        CHECK(close(n[4], nearest("-.000000962250448649376274181914634169929092746002918783544793"), 2e-5));   // d azimuth / d north
        CHECK(std::fabs(n[5]) < 1e-20);                                                   // d elevation / d north
        // and the three entries that are not zero print as bc's values rounded to four decimals ({:.4e}): 7.209794687e-7, -4.162576903e-7 and -9.622504486e-7
        const std::vector<std::string> printed = e4_texts_in(row.substr(prefix.size()));
        REQUIRE(printed.size() == 6);
        CHECK(printed[1] == "7.2098e-07");
        CHECK(printed[3] == "-4.1626e-07");
        CHECK(printed[4] == "-9.6225e-07");
    }
}

// the lines of the diurnal section's scan: the series scan, its bound, (with --check) the frozen fine scan, and F_a
void check_diurnal_scan(const std::vector<std::string>& v, bool check) {
    REQUIRE(v.size() == (check ? 4U : 3U));
    const std::string prefix = "series scan (10-degree grid, ends and centre, both signs): sup |full - pure| = (";
    INFO(v[0]);
    REQUIRE(starts_with(v[0], prefix));
    const std::string rest = v[0].substr(prefix.size());
    const std::vector<double> diff = numbers_in(between(rest, "", ")"));
    REQUIRE(diff.size() == 2);
    // the azimuth's own F_extra is 9e-6 (the Python's comment), the elevation's the frozen fine scan 2.0815e-02 to the 1e-3 its check asks
    CHECK(diff[0] > 8.5e-6);
    CHECK(diff[0] < 9.5e-6);
    CHECK(close(diff[1], 2.0815e-02, 1e-3));
    // the frozen 2.0815e-02 IS the scan's own output as the Python printed it ({:.4e}, four significant digits), so the elevation's supremum prints as exactly that, not merely within the 1e-3 of the tool's check:
    // a scan that dropped one sign of the observer's motion prints 2.0812e-02 and passes the check
    const std::string printed_pair = between(rest, "", ")");
    REQUIRE(printed_pair.find(", ") != std::string::npos);
    CHECK(printed_pair.substr(printed_pair.find(", ") + 2) == "2.0815e-02");
    const std::vector<double> pure = numbers_in(between(rest, "sup |pure| = (", ")"));
    REQUIRE(pure.size() == 2);
    // the pure third derivative of right ascension lies between F_pure(0; 30 deg) rho^3 = 2 / cos^3(30 deg) = 3.0792 and F_pure(1000 m; 30 deg) rho^3 = 3.0914 (the bound at the ends of the stencil), declination's below it
    CHECK(pure[0] > 3.0792);
    CHECK(pure[0] < 3.0914);
    CHECK(pure[1] > 2.0);
    CHECK(pure[1] < pure[0]);
    const std::string tail = "worst ratio to F_pure(h; 30 deg) ";
    REQUIRE(rest.find(tail) != std::string::npos);
    CHECK(rest.substr(rest.find(tail) + tail.size()) == "1.000000000000");   // the north axis at the centre attains the bound exactly
    CHECK(v[1] == "    the pure geometric formula at phi = 30 deg bounds the pure third derivatives of the scan (the north axis at the centre attains it exactly): yes");
    std::size_t k = 2;
    if (check) CHECK(v[k++] == "    the frozen fine scan 2.0815e-02 reproduced: ok");
    // F_a = max over the sizes of F_pure(h; 30 deg) rho^3 + 0.0219 = 3.09137537 + 0.0219 = 3.11327537 (bc), against the pure sup of the line above
    INFO(v[k]);
    REQUIRE(starts_with(v[k], "    F_a (the largest of the five sizes) = 3.1133 /rho^3 against the pure sup "));
    const std::vector<double> last = numbers_in(v[k]);
    REQUIRE(last.size() == 3);
    CHECK(within(last[2], pure[0], 5.1e-5, "the pure sup of the F_a line is the first of the scan line's"));
}

// Everything a run with these flags prints on the real tree: the recorded output's lines, the registered lines of the diurnal section, the numbers of the others, and the verdict.
void verify_output(const Result& r, bool scan, bool check) {
    CHECK(r.err.empty());
    REQUIRE_FALSE(r.out.empty());
    CHECK(r.out.back() == '\n');
    const std::vector<std::string> got = lines_of(r.out);
    const std::vector<std::string> head = derived_recorded(scan, check);
    std::size_t k = 0;
    expect_lines(slice(got, k, head.size()), head, "the recorded sections of the Python's output");
    k += head.size();
    expect_lines(slice(got, k, 12), diurnal_registered(), "the diurnal section's registered lines");
    k += 12;
    check_diurnal_controls(slice(got, k, 6));
    k += 6;
    if (check) {
        expect_lines(slice(got, k, 5), diurnal_check_lines(), "the diurnal section's checks");
        k += 5;
    }
    if (scan) {
        const std::size_t n = check ? 4 : 3;
        check_diurnal_scan(slice(got, k, n), check);
        k += n;
    }
    if (check) {
        REQUIRE(got.size() > k);
        CHECK(got[k] == recorded()[105]);   // "ok       every frozen number reproduced"
        ++k;
    }
    CHECK(got.size() == k);
}

// ---- a synthetic tree: the station of the measmod_reference tests' worked example (the reference point (6378138, 3, 2) m), the two files the registry section reads -------------------------------------------------

struct Tree {
    TempDir td{"odl-fd"};
    fs::path root = td.path();
    void put(const char* relative, const std::string& text) const {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, text);
    }
    Tree(const std::string& slrf, const std::string& ecc) {
        put(mr::kSlrfRelative, slrf);
        put(mr::kEccRelative, ecc);
    }
    [[nodiscard]] fd::Settings settings() const {
        fd::Settings s = fd::default_settings();
        s.root = root;
        return s;
    }
};

// a station on the equator at longitude 0 with no velocity and an eccentricity of (up 1, north 2, east 3) m: its marker is (6378137, 0, 0), its reference point (6378138, 3, 2)
Tree worked_tree() {
    return Tree(slrf_text("0.6378137000000000E+07", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00", "0.0000000000000000E+00"),
                ecc_text(ecc_row("00:000:00000", "1.0000", "2.0000", "3.0000", "70900513")));
}

struct SizingOracle {
    const char* name;
    const char* f;
    const char* nu;
    const char* hstar;
    const char* best;
    std::array<const char*, 5> t, n, eps;
};

// the frozen sizing at its three geometries (and the angle one at 30 degrees too) by bc at 60 digits, from F = 1.1547005383792515 / rho^2 (range) or 2 / (rho^3 cos^3 phi_max) (angle), nu = 3 ulp(rt) (range) or
// 3 (ulp(2 pi) + ulp(rt)/rho) (angle) with the ulps written out by hand (2^-29, 2^-30, 2^-30 and 2^-50), h* = (3 nu / F)^(1/3), best = h*^2 F / 6 + nu / h*, and T = h^2 F / 6, N = nu / h at the five sizes
// (C8_registered/fd_values.bc; the strings are what bc printed, from gen_sizing_oracle.awk)
const std::array<SizingOracle, 4> kSizing = {{
    {"range LAGEOS-like", ".000000000000026508276822296866391184573002754820936639118457", ".000000005587935447692871093750000000000000000000000000000000", "85.834861882913775363060095387389773014418843257847012220530716", ".000000000097651501821869910142979045665159582030823570915542",
     {".000000000000441804613704947773186409550045913682277318640950", ".000000000003976241523344529958677685950413223140495867768550", ".000000000044180461370494777318640955004591368227731864095000", ".000000000397624152334452995867768595041322314049586776855000", ".000000004418046137049477731864095500459136822773186409500000"},
     {".000000000558793544769287109375000000000000000000000000000000", ".000000000186264514923095703125000000000000000000000000000000", ".000000000055879354476928710937500000000000000000000000000000", ".000000000018626451492309570312500000000000000000000000000000", ".000000000005587935447692871093750000000000000000000000000000"},
     {".000000000559235349382992057148186409550045913682277318640950", ".000000000190240756446440233083677685950413223140495867768550", ".000000000100059815847423488256140955004591368227731864095000", ".000000000416250603826762566180268595041322314049586776855000", ".000000004423634072497170602957845500459136822773186409500000"}},
    {"range LEO-like", ".000000000000513200239279667333333333333333333333333333333333", ".000000002793967723846435546875000000000000000000000000000000", "25.371838001497226191942311357536573227626720693302020066290335", ".000000000165181236988914230253057247257606009459884796659050",
     {".000000000008553337321327788888888888888888888888888888888883", ".000000000076980035891950099999999999999999999999999999999950", ".000000000855333732132778888888888888888888888888888888888333", ".000000007698003589195009999999999999999999999999999999995000", ".000000085533373213277888888888888888888888888888888888833333"},
     {".000000000279396772384643554687500000000000000000000000000000", ".000000000093132257461547851562500000000000000000000000000000", ".000000000027939677238464355468750000000000000000000000000000", ".000000000009313225746154785156250000000000000000000000000000", ".000000000002793967723846435546875000000000000000000000000000"},
     {".000000000287950109705971343576388888888888888888888888888883", ".000000000170112293353497951562499999999999999999999999999950", ".000000000883273409371243244357638888888888888888888888888333", ".000000007707316814941164785156249999999999999999999999995000", ".000000085536167181001735324435763888888888888888888888833333"}},
    {"angle phi 40", ".000000000000000002574686511864680643099489026109570633131880", ".000000000000004992841695639071986079216003417968749999999999", "17.985184516402601839544382418464123128712856692316055639864980", ".000000000000000416412883427932210058442417418341997883518243",
     {".000000000000000042911441864411344051658150435159510552198000", ".000000000000000386202976779702096464923353916435594969782000", ".000000000000004291144186441134405165815043515951055219800000", ".000000000000038620297677970209646492335391643559496978200000", ".000000000000429114418644113440516581504351595105521980000000"},
     {".000000000000000499284169563907198607921600341796874999999999", ".000000000000000166428056521302399535973866780598958333333333", ".000000000000000049928416956390719860792160034179687499999999", ".000000000000000016642805652130239953597386678059895833333333", ".000000000000000004992841695639071986079216003417968749999999"},
     {".000000000000000542195611428318542659579750776956385552197999", ".000000000000000552631033301004496000897220697034553303115333", ".000000000000004341072603397525125026607203550130742719799999", ".000000000000038636940483622339886445932778321619392811533333", ".000000000000429119411485809079588567583567598523490729999999"}},
    {"angle phi 30", ".000000000000000001781945275276622729966508581796164986566672", ".000000000000004992841695639071986079216003417968749999999999", "20.332536607848053948875399687065970832599696003413605203137542", ".000000000000000368338820084448540849102037415102620454296332",
     {".000000000000000029699087921277045499441809696602749776111200", ".000000000000000267291791291493409494976287269424747985000800", ".000000000000002969908792127704549944180969660274977611120000", ".000000000000026729179129149340949497628726942474798500080000", ".000000000000296990879212770454994418096966027497761112000000"},
     {".000000000000000499284169563907198607921600341796874999999999", ".000000000000000166428056521302399535973866780598958333333333", ".000000000000000049928416956390719860792160034179687499999999", ".000000000000000016642805652130239953597386678059895833333333", ".000000000000000004992841695639071986079216003417968749999999"},
     {".000000000000000528983257485184244107363410038399624776111199", ".000000000000000433719847812795809030950154050023706318334133", ".000000000000003019837209084095269804973129694454665111119999", ".000000000000026745821934801471189451226113620534694333413333", ".000000000000296995872054466094066404176182030915729861999999"}},
}};

using Quad = std::array<double, 4>;
Quad q(double a, double b, double c, double d) { return {a, b, c, d}; }

bool series_close(const Quad& got, const Quad& want, double tolerance) {
    for (std::size_t i = 0; i < 4; ++i) {
        if (!(std::fabs(got[i] - want[i]) <= tolerance)) return false;
    }
    return true;
}

}  // namespace

// =====================================================================================================================================================================================================
// THE FUNCTIONS
// =====================================================================================================================================================================================================

TEST_CASE("sizing: the frozen sizing of SPEC-measmod 6.2 from its formulas, against bc", "[measmod_fd_sizing][behaviour]") {
    const std::array<fd::Sizing, 4> s = {fd::sizing(6.6e6, 1.227e7, true), fd::sizing(1.5e6, 6.92e6, true), fd::sizing(1.2e6, 7.2e6, false), fd::sizing(1.2e6, 7.2e6, false, 30.0)};
    for (std::size_t g = 0; g < 4; ++g) {
        const SizingOracle& o = kSizing[g];
        INFO(o.name);
        CHECK(close(s[g].f, nearest(o.f), 4e-15));
        CHECK(close(s[g].nu, nearest(o.nu), 4e-15));
        CHECK(close(s[g].best_step, nearest(o.hstar), 2e-14));
        CHECK(close(s[g].best, nearest(o.best), 2e-14));
        for (std::size_t i = 0; i < 5; ++i) {
            INFO("h = " << fd::kSteps[i]);
            CHECK(close(s[g].truncation[i], nearest(o.t[i]), 4e-15));
            CHECK(close(s[g].noise[i], nearest(o.n[i]), 4e-15));
            CHECK(close(s[g].bound[i], nearest(o.eps[i]), 4e-15));
        }
    }
    // nu of a range is three ulp of the target's distance, exactly: ulp(1.227e7) = 2^-29 and ulp(6.92e6) = 2^-30
    CHECK(s[0].nu == std::ldexp(3.0, -29));
    CHECK(s[1].nu == std::ldexp(3.0, -30));
    // the angle's phi_max enters the angle's F and not the range's
    CHECK(fd::sizing(6.6e6, 1.227e7, true, 10.0).f == s[0].f);
    CHECK(s[2].f != s[3].f);
    CHECK(s[2].nu == s[3].nu);
    // the five sizes of the gate
    CHECK(fd::kSteps == std::array<double, 5>{10.0, 30.0, 100.0, 300.0, 1000.0});
}

TEST_CASE("Series: the truncated power series of order four, operation by operation, on numbers that are exact in binary", "[measmod_fd_sizing][behaviour]") {
    using S = fd::Series;
    const S a(2.0, 1.0);   // 2 + h
    CHECK((a * a).c == q(4.0, 4.0, 1.0, 0.0));
    CHECK((a + a).c == q(4.0, 2.0, 0.0, 0.0));
    CHECK((a - a).c == q(0.0, 0.0, 0.0, 0.0));
    // 1 / (2 + h) = 1/2 - h/4 + h^2/8 - h^3/16, and (2 + h) times it is 1
    CHECK(a.inv().c == q(0.5, -0.25, 0.125, -0.0625));
    CHECK((a / a).c == q(1.0, 0.0, 0.0, 0.0));
    CHECK(S(1.0, 1.0).inv().c == q(1.0, -1.0, 1.0, -1.0));
    // sqrt(4 + 4h) = 2 sqrt(1 + h) = 2 + h - h^2/4 + h^3/8
    CHECK(S(4.0, 4.0).sqrt().c == q(2.0, 1.0, -0.25, 0.125));
    // the derivative and the integral
    const S p(q(1.0, 2.0, 3.0, 4.0));
    CHECK(p.deriv().c == q(2.0, 6.0, 12.0, 0.0));
    CHECK(p.integ(5.0).c == q(5.0, 1.0, 1.0, 1.0));
    CHECK(S(7.0).c == q(7.0, 0.0, 0.0, 0.0));
    // a float beside a series is the series [float, 0, 0, 0]: every combination
    CHECK((3.0 - S(1.0, 1.0)).c == q(2.0, -1.0, 0.0, 0.0));
    CHECK((S(1.0, 1.0) - 3.0).c == q(-2.0, 1.0, 0.0, 0.0));
    CHECK((1.5 + a).c == q(3.5, 1.0, 0.0, 0.0));
    CHECK((a + 1.5).c == q(3.5, 1.0, 0.0, 0.0));
    CHECK((2.0 * a).c == q(4.0, 2.0, 0.0, 0.0));
    CHECK((a * 2.0).c == q(4.0, 2.0, 0.0, 0.0));
    CHECK((a / 2.0).c == q(1.0, 0.5, 0.0, 0.0));
    CHECK((1.0 / a).c == q(0.5, -0.25, 0.125, -0.0625));
    CHECK((8.0 / S(4.0, 4.0)).c == q(2.0, -2.0, 2.0, -2.0));
    // the order: the h^4 term is dropped, the products' coefficients are sums over every pair that adds up to the degree
    const S b(q(1.0, 2.0, 3.0, 4.0));
    CHECK((b * b).c == q(1.0, 4.0, 10.0, 20.0));   // (1 + 2h + 3h^2 + 4h^3)^2 = 1 + 4h + 10h^2 + 20h^3 + ...
    // the signs of zero are the Python's: a product accumulates into +0, a sum keeps the sign of two zeros, a difference of -0 and +0 is -0
    CHECK_FALSE(std::signbit((S(0.0) * S(-1.0)).c[0]));
    CHECK(std::signbit((S(-0.0) + S(-0.0)).c[0]));
    CHECK(std::signbit((S(-0.0) - S(0.0)).c[0]));
    // the convolutions of the inverse and of the square root run over the Python's indices: the sum for a coefficient never holds the leading coefficient times the coefficient being computed (still 0): with an infinite
    // leading coefficient that product would be NaN (inf x 0)
    const double infinity = std::numeric_limits<double>::infinity();
    const S big(infinity, 1.0);
    const S inverse = big.inv();
    for (std::size_t i = 0; i < 4; ++i) {
        INFO("inv, coefficient " << i);
        CHECK(inverse.c[i] == 0.0);
    }
    const S root = big.sqrt();
    CHECK(root.c[0] == infinity);
    for (std::size_t i = 1; i < 4; ++i) {
        INFO("sqrt, coefficient " << i);
        CHECK(root.c[i] == 0.0);
    }
}

TEST_CASE("atan_series and third_derivatives: arctan and the third derivatives of the right ascension and declination, by hand", "[measmod_fd_sizing][behaviour]") {
    using S = fd::Series;
    // atan(h) = h - h^3/3 + ...
    CHECK(series_close(fd::atan_series(S(0.0, 1.0)).c, q(0.0, 1.0, 0.0, -1.0 / 3.0), 1e-15));
    // atan(1 + h) = pi/4 + h/2 - h^2/4 + h^3/12 + ...  (f' = 1/2, f'' = -1/2, f''' = 1/2 at 1)
    CHECK(series_close(fd::atan_series(S(1.0, 1.0)).c, q(std::numbers::pi / 4, 0.5, -0.25, 1.0 / 12.0), 1e-15));
    // a line of sight along x displaced along y: the position is (1, h, 0): right ascension atan(h), third derivative -2, declination 0
    {
        const std::array<double, 2> d = fd::third_derivatives({1.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
        CHECK(within(d[0], -2.0, 1e-15, "RA"));
        CHECK(d[1] == 0.0);
    }
    // displaced along z: (1, 0, h): right ascension 0, declination atan(h)
    {
        const std::array<double, 2> d = fd::third_derivatives({1.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
        CHECK(d[0] == 0.0);
        CHECK(within(d[1], -2.0, 1e-15, "Dec"));
    }
    // a line of sight at declination 30 deg displaced east: the position is (cos a, h, sin a): right ascension atan(h / cos a), whose third derivative is -2 / cos^3 a; the declination is even in h: 0
    {
        const double x = std::cos(30.0 * std::numbers::pi / 180);
        const std::array<double, 2> d = fd::third_derivatives({x, 0.0, std::sin(30.0 * std::numbers::pi / 180)}, {0.0, 1.0, 0.0});
        CHECK(close(d[0], -2.0 / (x * x * x), 1e-14));
        CHECK(d[1] == 0.0);
    }
    // displaced in the plane of the line of sight and the pole: (1 + 0.6 h, 0, 0.8 h): right ascension 0; declination atan(0.8 h / (1 + 0.6 h)) = u - u^3/3, u = 0.8 h - 0.48 h^2 + 0.288 h^3,
    // whose h^3 coefficient is 0.288 - 0.512/3 = 0.352/3 = 0.117333..., so the third derivative is 6 times it: 0.704
    {
        const std::array<double, 2> d = fd::third_derivatives({1.0, 0.0, 0.0}, {0.6, 0.0, 0.8});
        CHECK(d[0] == 0.0);
        CHECK(close(d[1], 0.704, 1e-14));
    }
}

TEST_CASE("scan: the largest third derivatives of right ascension and declination for |dec| <= 30 degrees, as the Python recorded them", "[measmod_fd_sizing][behaviour]") {
    // R1 line 26: "series scan, |dec| <= 30 deg: max |f'''| RA 3.0750, Dec 2.2295 (rho = 1); analytic RA bound 2/cos^3 = 3.0792"
    const std::array<double, 2> s = fd::scan();
    CHECK(within(s[0], 3.0750, 5.1e-5, "RA"));
    CHECK(within(s[1], 2.2295, 5.1e-5, "Dec"));
    CHECK(s[0] <= 2.0 / std::pow(std::cos(30.0 * std::numbers::pi / 180), 3));   // 3.0792: the analytic bound
    CHECK(s[1] <= s[0]);
    // the argument is unused, as the Python's was
    CHECK(fd::scan(5.0) == s);
}

TEST_CASE("v_station_max, f_geo, nu_chain and eps_amended against bc", "[measmod_fd_sizing][behaviour]") {
    // 7.2921150e-5 rad/s x 6378137 m (bc: 465.101084897550)
    CHECK(close(fd::v_station_max(), nearest("465.101084897550"), 5e-16));
    // F_geo(h) = 1.1547005383792515 (1 + kappa)^3 / (rho (1 - kappa) - h)^2, kappa = (speed + 2 v_station) / c
    CHECK(close(fd::f_geo(0.0, 6.6e6, 5700.0), nearest(".000000000000026511208259678860367496603689694993714782609834"), 4e-15));
    CHECK(close(fd::f_geo(10.0, 6.6e6, 5700.0), nearest(".000000000000026511288598632949362961361419102177182548242918"), 4e-15));
    CHECK(close(fd::f_geo(1000.0, 6.6e6, 5700.0), nearest(".000000000000026519243963119010986580969990923195072532159971"), 4e-15));
    CHECK(close(fd::f_geo(0.0, 1.5e6, 7500.0), nearest(".000000000000513272400430642274050527830872751433559865486321"), 4e-15));
    CHECK(close(fd::f_geo(10.0, 1.5e6, 7500.0), nearest(".000000000000513279244323538010306829356203700343630949779045"), 4e-15));
    CHECK(close(fd::f_geo(1000.0, 1.5e6, 7500.0), nearest(".000000000000513957467886710675759972470345482206266660275140"), 4e-15));
    // the chain's bound of one evaluation: 3 x 2^-45 rad x the axis distance x |cos(el) cos(az)| x (1/2 in event 2, 1 in event 1); and the cosine itself
    const auto lageos2 = fd::nu_chain(2, 5580552.4, 52.0, 105.0);
    const auto lageos1 = fd::nu_chain(1, 5580552.4, 52.0, 105.0);
    CHECK(close(lageos2.first, nearest(".000000037910267934757773989660483544451396068909234933425153"), 2e-15));
    CHECK(close(lageos1.first, nearest(".000000075820535869515547979320967088902792137818469866850306"), 2e-15));
    CHECK(close(lageos2.second, nearest(".159344915150196023739458861429485789426932785569922072387126"), 4e-15));
    CHECK(lageos2.second == lageos1.second);
    const auto leo2 = fd::nu_chain(2, 5580552.4, 15.0, 0.0);
    const auto leo1 = fd::nu_chain(1, 5580552.4, 15.0, 0.0);
    CHECK(close(leo2.first, nearest(".000000229806560474206795620311578970420394269200555642383632"), 2e-15));
    CHECK(close(leo1.first, nearest(".000000459613120948413591240623157940840788538401111284767264"), 2e-15));
    CHECK(close(leo2.second, nearest(".965925826289068286749743199728897367633904839008404550402343"), 4e-15));
    // the cosine is the modulus: a line of sight to the west of the east-west axis has the same
    CHECK(fd::nu_chain(2, 5580552.4, 52.0, 255.0).second == fd::nu_chain(2, 5580552.4, 52.0, 255.0).second);
    CHECK(close(fd::nu_chain(2, 5580552.4, 52.0, -105.0).second, lageos2.second, 1e-15));
    // eps(h) = h^2 (F_geo(h) + 1.01 F_trop(h)) / 6 + nu / h, for the LAGEOS-like case's frozen F_trop and nu = 3 ulp(1.227e7) = 3 x 2^-29
    const std::array<double, 5> f_trop = {9.6904e-20, 9.6905e-20, 9.6910e-20, 9.6924e-20, 9.6973e-20};
    const double nu = std::ldexp(3.0, -29);
    CHECK(close(fd::eps_amended(0, f_trop, 6.6e6, 5700.0, nu), nearest(".000000000559235401210481658531049356023651702953042470715300"), 4e-15));
    CHECK(close(fd::eps_amended(4, f_trop, 6.6e6, 5700.0, nu), nearest(".000000004425478253089194702190578331820532512088693328500000"), 4e-15));
    CHECK(close(fd::eps_amended(0, f_trop, 6.6e6, 5700.0, nu + lageos2.first), nearest(".000000004350262194686259057497097710468791309843965964057815"), 1e-14));
}

TEST_CASE("fcula, zenith_total, refco and the two Decimal checks against the published cases and the 60-digit values", "[measmod_fd_sizing][behaviour]") {
    Sixty sixty;
    // the IERS FCUL_A prolog's printed case: latitude 30.67166667 deg, height 2075 m, T = 300.15 K (27 C), elevation 15 deg, FCULa = 3.800243667312344087
    const double sin15 = (std::sqrt(6.0) - std::sqrt(2.0)) / 4;
    const double cphi = std::cos(30.67166667 * std::numbers::pi / 180);
    CHECK(within(fd::fcula(sin15, cphi, 2075.0, 27.0), 3.800243667312344087, 1e-14, "FCULa at the IERS case"));
    // at the zenith (s = 1) the mapping function is 1, whatever the station
    CHECK(fd::fcula(1.0, cphi, 2075.0, 27.0) == 1.0);
    CHECK(fd::fcula(1.0, 0.3, 17.0, -40.0) == 1.0);
    // and it grows toward the horizon
    CHECK(fd::fcula(0.5, cphi, 2075.0, 27.0) > fd::fcula(0.8, cphi, 2075.0, 27.0));
    // the Decimal evaluation of the same: the tool's own number is the printed value minus its 60 digits, -2.37e-16 (R1 line 31); and the number follows the text it is given
    {
        const double d = fd::iers_fcula_check();
        CHECK(d < -2.365e-16);
        CHECK(d > -2.375e-16);
        // V0 = 3.800243667312344087 - 2.37e-16 = 3.80024366731234385 (to 5e-19): the difference with another text is V0 minus it, written out in decimals:
        // 3.80024366731234385 - 3.80024366731244 = -9.615e-14 and 3.80024366731234385 - 3.8002436673123 = 4.385e-14
        CHECK(within(fd::iers_fcula_check("3.80024366731244"), -9.615e-14, 1e-18, "against 3.80024366731244"));
        CHECK(within(fd::iers_fcula_check("3.8002436673123"), 4.385e-14, 1e-18, "against 3.8002436673123"));
    }
    // the zenith delay of TN36 (9.3)-(9.7) in double precision at the IERS FCUL_ZD_HPA prolog's printed inputs: the 60-digit value of measmod_reference's zenith section (bc: 1.935229724967973192...)
    CHECK(close(fd::zenith_total(0.5353215704657051, 2010.344, 798.4188, 14.322, 0.532), nearest("1.935229724967973192256266066647167800440981039923429333407885"), 1e-13));
    // eraRefco at ERFA's own published case (t_erfa_c.c: 800 hPa, 10 C, 90 %, 0.4 um): A = 0.2264949956241415009e-3 to 1e-15, B = -0.2598658261729343970e-6 to 1e-18
    {
        const auto [a, b] = fd::refco(800.0, 10.0, 0.9, 0.4);
        CHECK(within(a, 0.2264949956241415009e-3, 1e-15, "A of ERFA's case"));
        CHECK(within(b, -0.2598658261729343970e-6, 1e-18, "B of ERFA's case"));
        CHECK(fd::refco_check() < 1e-3);   // R1 line 87: "0.000 of ERFA's tolerances"
        // the unit of the check is ERFA's own tolerance: a published value off by 3e-15 (A) or 5e-18 (B) is 3 or 5 units
        CHECK(within(fd::refco_check(0.2264949956241415009e-3 + 3e-15, -0.2598658261729343970e-6), 3.0, 1e-3, "A off by 3e-15"));
        CHECK(within(fd::refco_check(0.2264949956241415009e-3, -0.2598658261729343970e-6 + 5e-18), 5.0, 1e-3, "B off by 5e-18"));
        // the larger of the two
        CHECK(within(fd::refco_check(0.2264949956241415009e-3 - 2e-15, -0.2598658261729343970e-6 - 7e-18), 7.0, 1e-3, "the larger of the two"));
    }
    // the first normal point's weather (976.7 hPa, 37.15 C, rh 0.12, 0.532 um): A = 2.484383e-04, B = -3.123796e-07 (R1 line 72)
    {
        const auto [a, b] = fd::refco(976.7, 37.15, 0.12, 0.532);
        CHECK(close(a, 2.484383e-04, 1e-6));
        CHECK(close(b, -3.123796e-07, 1e-6));
    }
}

TEST_CASE("f_pure, f_pure_at, ceil3, unit_of_angles and sphere_half", "[measmod_fd_sizing][behaviour]") {
    // F_pure(h; phi) = 2 / [(rho - h)^3 cos^3(phi + asin(h / rho))], rho = 1.2e6 m, at phi_max = 40 deg (f_pure) and at any phi (f_pure_at): bc
    CHECK(close(fd::f_pure(10.0), nearest(".000000000000000002574804892936699236971514351947797044331717"), 4e-15));
    CHECK(close(fd::f_pure(1000.0), nearest(".000000000000000002586558819656679266756074855854846025749446"), 4e-15));
    CHECK(close(fd::f_pure_at(10.0, 30.0), nearest(".000000000000000001782015545891744125350182125967981515951252"), 4e-15));
    CHECK(close(fd::f_pure_at(1000.0, 30.0), nearest(".000000000000000001788990378336539893277949305331706271154875"), 4e-15));
    CHECK(close(fd::f_pure_at(100.0, 40.0, 1.5e6), nearest(".000000000000000001318724481786515214060163237527718178641939"), 4e-15));
    CHECK(fd::f_pure(10.0) == fd::f_pure_at(10.0, 40.0));
    CHECK(fd::f_pure(10.0, 1.5e6) == fd::f_pure_at(10.0, 40.0, 1.5e6));
    // ceil3: x rounded UP to three significant digits, with the 1e-9 allowance of its comparison
    CHECK(within(fd::ceil3(1.05 * 2.495171e-4), 2.62e-4, 1e-18, "geometric"));
    CHECK(within(fd::ceil3(1.05 * 1.358137e-3), 1.43e-3, 1e-17, "astrometric"));
    CHECK(within(fd::ceil3(1.05 * 9.271765e-2), 9.74e-2, 1e-16, "refracted"));
    CHECK(within(fd::ceil3(1.05 * 2.0815e-2), 2.19e-2, 1e-16, "diurnal"));
    CHECK(fd::ceil3(1.0) == 1.0);
    CHECK(within(fd::ceil3(1.001), 1.01, 1e-15, "1.001"));
    CHECK(fd::ceil3(1000.0) == 1000.0);
    CHECK(fd::ceil3(999.5) == 1000.0);
    CHECK(within(fd::ceil3(0.00123), 0.00123, 1e-18, "0.00123 (a float's noise is allowed for)"));
    CHECK(within(fd::ceil3(0.0012300001), 0.00124, 1e-18, "0.0012300001"));
    CHECK(fd::ceil3(2.62e-4 * (1 + 1e-12)) == fd::ceil3(2.62e-4));   // the allowance: x / scale - 1e-9 is not above the whole number
    CHECK(within(fd::ceil3(2.62e-4 * (1 + 1e-8)), 2.63e-4, 1e-18, "more than the allowance"));
    // unit_of_angles(first, second): (cos(second) cos(first), cos(second) sin(first), sin(second))
    {
        const std::array<double, 3> x = fd::unit_of_angles(0.0, 0.0);
        CHECK(x == std::array<double, 3>{1.0, 0.0, 0.0});
        const std::array<double, 3> y = fd::unit_of_angles(90.0, 0.0);
        CHECK(within(y[0], 0.0, 1e-15, "x"));
        CHECK(y[1] == 1.0);
        CHECK(y[2] == 0.0);
        const std::array<double, 3> z = fd::unit_of_angles(0.0, 90.0);
        CHECK(within(z[0], 0.0, 1e-15, "x"));
        CHECK(z[1] == 0.0);
        CHECK(z[2] == 1.0);
        const std::array<double, 3> d = fd::unit_of_angles(45.0, 0.0);
        CHECK(within(d[0], std::sqrt(0.5), 2e-16, "x"));
        CHECK(within(d[1], std::sqrt(0.5), 2e-16, "y"));
        const std::array<double, 3> e = fd::unit_of_angles(-45.0, 30.0);
        CHECK(within(e[0], std::sqrt(0.5) * std::sqrt(0.75), 4e-16, "x"));
        CHECK(within(e[1], -std::sqrt(0.5) * std::sqrt(0.75), 4e-16, "y"));
        CHECK(within(e[2], 0.5, 4e-16, "z"));
    }
    // sphere_half(step): the directions of a half-sphere on a grid of `step` degrees, theta outermost (0 to 180) and phi inside (0 to 360, the last cell not repeated), those with z > 0 or, on the equator, with x > 0 or
    // (x = 0 and y > 0): by hand, 6 at 90 degrees (theta 0: 4 copies of the pole; theta 90: phi 0 and phi 90), 156 at 15 degrees (6 x 24 above the equator, 12 on it), 342 at 10 degrees (9 x 36 + 18)
    CHECK(fd::sphere_half(90.0).size() == 6);
    CHECK(fd::sphere_half(15.0).size() == 156);
    CHECK(fd::sphere_half(10.0).size() == 342);
    {
        const std::vector<std::array<double, 3>> h = fd::sphere_half(90.0);
        REQUIRE(h.size() == 6);
        for (std::size_t i = 0; i < 4; ++i) {   // theta = 0: the pole, once for each phi
            CHECK(within(h[i][0], 0.0, 1e-15, "x"));
            CHECK(within(h[i][1], 0.0, 1e-15, "y"));
            CHECK(h[i][2] == 1.0);
        }
        CHECK(h[4][0] == 1.0);   // theta = 90, phi = 0
        CHECK(within(h[4][1], 0.0, 1e-15, "y"));
        CHECK(within(h[4][2], 0.0, 1e-15, "z"));
        CHECK(within(h[5][0], 0.0, 1e-15, "x"));   // theta = 90, phi = 90
        CHECK(h[5][1] == 1.0);
    }
    // the number of cells is the Python's round(), which rounds a half to the even number: 360 / 80 = 4.5 cells make 4 (and not 5): theta 0 (the pole, 4 copies) and 80 degrees, 4 phi each; theta 160 is below the equator
    {
        const std::vector<std::array<double, 3>> h = fd::sphere_half(80.0);
        REQUIRE(h.size() == 8);
        for (std::size_t i = 0; i < 4; ++i) CHECK(h[i][2] == 1.0);
        for (std::size_t i = 0; i < 4; ++i) {
            const double phi = 80.0 * static_cast<double>(i) * std::numbers::pi / 180;
            CHECK(within(h[4 + i][0], std::sin(80.0 * std::numbers::pi / 180) * std::cos(phi), 1e-15, "x"));
            CHECK(within(h[4 + i][1], std::sin(80.0 * std::numbers::pi / 180) * std::sin(phi), 1e-15, "y"));
            CHECK(within(h[4 + i][2], std::cos(80.0 * std::numbers::pi / 180), 1e-15, "z"));
        }
    }
    CHECK(fd::sphere_half(40.0).size() == 27);   // 180 / 40 = 4.5 makes 4 rows (theta 0 to 160), 9 columns each: the rows 0, 40 and 80 are above the equator
    for (const double step : {10.0, 15.0, 30.0}) {
        for (const std::array<double, 3>& e : fd::sphere_half(step)) {
            CHECK(within(e[0] * e[0] + e[1] * e[1] + e[2] * e[2], 1.0, 4e-16, "unit"));
            CHECK((e[2] > 1e-12 || (std::fabs(e[2]) <= 1e-12 && (e[0] > 1e-12 || (std::fabs(e[0]) <= 1e-12 && e[1] > 0)))));
        }
    }
}

TEST_CASE("station_inputs and atmosphere_inputs: the real station as R1 printed it, and the worked example's by bc", "[measmod_fd_sizing][behaviour]") {
    Sixty sixty;
    // R1 line 30: axis distance 5580552.4 m, ZTD = 2.364942 m, lat -29.0465 deg, H 244.5 m, t = 37.15 C (printed to the digits shown)
    {
        const fd::Atmosphere atm = fd::atmosphere_inputs(mr::default_root());
        const fd::StationInputs st = fd::station_inputs(mr::default_root());
        CHECK(within(atm.r_perp, 5580552.4, 0.0500001, "axis distance"));
        CHECK(within(atm.lat * 180 / std::numbers::pi, -29.0465, 5.1e-5, "latitude"));
        CHECK(within(atm.height, 244.5, 0.0500001, "height"));
        CHECK(within(atm.t_c, 37.15, 5.1e-3, "temperature"));
        CHECK(within(atm.z, 2.364942, 5.1e-7, "zenith delay"));
        CHECK(atm.r_perp == st.r_perp);
        CHECK(atm.lat == st.lat);
        CHECK(atm.height == st.height);
    }
    // the worked example (the reference point (6378138, 3, 2)): hypot(6378138, 3), the latitude and height by bc's iteration run to convergence, the first normal point's weather, ZTD by bc (atm_worked.bc)
    {
        const Tree t = worked_tree();
        const fd::Atmosphere atm = fd::atmosphere_inputs(t.root);
        CHECK(atm.r_perp == nearest("6378138.000000705535063681555825561779618359298904105695134944891299"));
        CHECK(within(atm.lat, 3.15684450752979436598e-7, 2e-19, "latitude"));
        CHECK(within(atm.height, 1.000001021219514, 5e-9, "height"));
        CHECK(within(atm.t_c, 37.15, 1e-12, "temperature"));
        CHECK(close(atm.z, nearest("2.367753736616478549568454074126271038729316413847069367209853"), 1e-13));
    }
    // a station that is not there is refused, naming what is wanted
    {
        TempDir empty("odl-fd");
        CHECK_THROWS_AS(fd::atmosphere_inputs(empty.path()), mr::MissingInput);
        CHECK_THROWS_AS(fd::station_inputs(empty.path()), mr::MissingInput);
    }
}

TEST_CASE("trop_shapiro_scan and coupling_scan on the real station: the stencil bounds R1 printed", "[measmod_fd_sizing][real_tree]") {
    Sixty sixty;
    const fd::Atmosphere atm = fd::atmosphere_inputs(mr::default_root());
    // R1 lines 34 and 36-40: the LAGEOS-like case (rho 6.6e6 m, elevation 52 deg): F_trop(sup) at the five sizes, Shapiro alone 3.503e-23, the atmosphere alone 9.694e-20
    {
        const fd::TropScan t = fd::trop_shapiro_scan(6.6e6, 52.0, atm);
        const std::array<double, 5> want = {9.6904e-20, 9.6905e-20, 9.6910e-20, 9.6924e-20, 9.6973e-20};
        for (std::size_t i = 0; i < 5; ++i) CHECK(close(t.sup[i], want[i], 1e-4));
        CHECK(close(t.sup_shapiro, 3.503e-23, 1e-3));
        CHECK(close(t.sup_atmosphere, 9.694e-20, 1e-3));
    }
    // R1 lines 52 and 54-58: the LEO-like case (rho 1.5e6 m, elevation 15 deg)
    {
        const fd::TropScan t = fd::trop_shapiro_scan(1.5e6, 15.0, atm);
        const std::array<double, 5> want = {7.7299e-16, 7.7314e-16, 7.7365e-16, 7.7512e-16, 7.8028e-16};
        for (std::size_t i = 0; i < 5; ++i) CHECK(close(t.sup[i], want[i], 1e-4));
        CHECK(close(t.sup_shapiro, 8.009e-22, 1e-3));
        CHECK(close(t.sup_atmosphere, 7.803e-16, 1e-3));
    }
    // R1 lines 45 and 66: the largest ratio over the cone of the closed-form two-way range's third derivative to F_geo(h): 0.999888 (LAGEOS-like, across, event 2), 0.999854 (LEO-like, along, event 1)
    {
        const std::array<double, 5> sup = fd::coupling_scan(6.6e6, 52.0, 105.0, 5700.0, 0.0, 2);
        double worst = 0.0;
        for (std::size_t i = 0; i < 5; ++i) worst = std::max(worst, sup[i] / fd::f_geo(fd::kSteps[i], 6.6e6, 5700.0));
        CHECK(within(worst, 0.999888, 5.1e-7, "LAGEOS-like, across, event 2"));
    }
    {
        const std::array<double, 5> sup = fd::coupling_scan(1.5e6, 15.0, 0.0, 7500.0, 1.0, 1);
        double worst = 0.0;
        for (std::size_t i = 0; i < 5; ++i) worst = std::max(worst, sup[i] / fd::f_geo(fd::kSteps[i], 1.5e6, 7500.0));
        CHECK(within(worst, 0.999854, 5.1e-7, "LEO-like, along, event 1"));
    }
}

// =====================================================================================================================================================================================================
// THE FOUR MODES ON THE REAL TREE
// =====================================================================================================================================================================================================

TEST_CASE("--scan --check on the real tree: the Python's recorded output byte for byte, then the diurnal section registered for it", "[measmod_fd_sizing][real_tree]") {
    const Result& r = real_scan_check();
    CHECK(r.code == 0);
    verify_output(r, true, true);
    CHECK(lines_of(r.out).size() == 133);
    // the recorded part, whole: the first 105 lines of the record are the first 105 of the output, and its last is the output's last
    const std::vector<std::string> got = lines_of(r.out);
    REQUIRE(got.size() == 133);
    REQUIRE(recorded().size() == 106);
    for (std::size_t n = 0; n < 105; ++n) {
        INFO("recorded line " << n + 1);
        CHECK(got[n] == recorded()[n]);
    }
    CHECK(got.back() == recorded()[105]);
}

TEST_CASE("--check on the real tree: the recorded output without its scan lines, the frozen F_trop in use", "[measmod_fd_sizing][real_tree]") {
    const Result& r = real_check();
    CHECK(r.code == 0);
    verify_output(r, false, true);
    CHECK(lines_of(r.out).size() == 104);
}

TEST_CASE("--scan on the real tree: the recorded output without its checks", "[measmod_fd_sizing][real_tree]") {
    const Result& r = real_scan();
    CHECK(r.code == 0);
    verify_output(r, true, false);
    CHECK(lines_of(r.out).size() == 110);
}

TEST_CASE("no flag on the real tree: the sizing, the amended and angle sections on the frozen F_trop, the diurnal section", "[measmod_fd_sizing][real_tree]") {
    const Result& r = real_plain();
    CHECK(r.code == 0);
    verify_output(r, false, false);
    CHECK(lines_of(r.out).size() == 83);
    // nothing of the checks and nothing of the scan
    for (const std::string& line : lines_of(r.out)) {
        CHECK(line.find("frozen B_pred") == std::string::npos);
        CHECK(line.find("reproduced") == std::string::npos);
        CHECK(line.find("NOT") == std::string::npos);
        CHECK(line.find("Shapiro alone") == std::string::npos);
        CHECK(line.find("closed-form two-way") == std::string::npos);
        CHECK(line.find("series scan") == std::string::npos);
        CHECK(line.find("series guard") == std::string::npos);
    }
}

// =====================================================================================================================================================================================================
// A STATION THAT IS NOT THE FIRST NORMAL POINT'S, REFUSED INPUT, THE COMMAND LINE
// =====================================================================================================================================================================================================

TEST_CASE("another station: the amended section follows it, the frozen checks that depend on it fire, the sections that do not stay as they were", "[measmod_fd_sizing][behaviour]") {
    const Tree t = worked_tree();
    const Result r = run_with(t.settings(), {"--check"});
    const std::vector<std::string> got = lines_of(r.out);
    const std::vector<std::string> real = lines_of(real_check().out);
    REQUIRE(got.size() == real.size());
    CHECK(r.code == 1);
    CHECK(r.err.empty());
    // In the output of --check the 25 lines of the sizing come first, then a blank line (index 25), the amended section's title (26), the chain's floor (27), the IERS check (28) and the check of the ulp (29).
    // The floor is at the axis distance 6378138.0 m (bc, worked_floor.bc: 2^-45 x 6378138.0000007 = 1.81277...e-7 m), the ZTD that of atm_worked.bc, the latitude 1.8e-5 deg, the height 1.0 m.
    REQUIRE(got.size() > 29);
    CHECK(got[26] == "=== THE AMENDED RANGE SIZING (2026-10-06, made after the first run's miss) ===");
    CHECK(got[27] == "the chain's floor: 2^-45 rad = 2.8422e-14 rad, axis distance 6378138.0 m, one ulp = 1.8128e-07 m, three ulp = 5.4383e-07 m;  ZTD = 2.367754 m, lat 0.0000 deg, H 1.0 m, t = 37.15 C");
    // the lines that stay as they were: the frozen sizing, the IERS check (it does not depend on the station), and everything from the angle sizing on (the blank line before its title to the verdict, which differs)
    for (std::size_t i = 0; i < 26; ++i) CHECK(got[i] == real[i]);
    CHECK(got[28] == real[28]);
    const auto index_of = [](const std::vector<std::string>& lines, const std::string& title) {
        std::size_t k = 0;
        while (k < lines.size() && lines[k] != title) ++k;
        return k;
    };
    const std::string angle_title = "=== THE ANGLE SIZING (item 6: written and committed before the angle gate's first run) ===";
    const std::size_t angle = index_of(got, angle_title);
    REQUIRE(angle < got.size());
    CHECK(angle == index_of(real, angle_title));
    for (std::size_t i = angle - 1; i + 1 < got.size(); ++i) CHECK(got[i] == real[i]);
    // the checks that fire: the one ulp of the chain's angle (1.8128e-7 against 1.59e-7), and each case's amended eps / nu_chain / B_pred (nu_chain is 14 % larger); the other checks of the section stay ok
    CHECK(got[29] == "one ulp of the chain's angle at the station = 1.813e-07 m against the 1.59e-7 m predicted before it was measured: NOT REPRODUCED");
    const std::vector<std::string> fired = [&] {
        std::vector<std::string> out;
        for (const std::string& line : got) {
            if (ends_with(line, "NOT REPRODUCED")) out.push_back(line);
        }
        return out;
    }();
    REQUIRE(fired.size() == 3);
    CHECK(fired[1] == "    the frozen amended eps(h), nu_chain and B_pred reproduced: NOT REPRODUCED");
    CHECK(fired[2] == "    the frozen amended eps(h), nu_chain and B_pred reproduced: NOT REPRODUCED");
    CHECK(got.back() == "FAILED   3 frozen number(s) not reproduced");
}

TEST_CASE("an input file that is missing, not UTF-8 or not what is read is refused, exit 2, after the sizing has been printed", "[measmod_fd_sizing][behaviour]") {
    // with --check the first 25 lines are the recorded ones (the three sizings and the machine round-off line)
    const std::vector<std::string> first25 = slice(recorded(), 0, 25);
    {
        TempDir empty("odl-fd");
        fd::Settings s = fd::default_settings();
        s.root = empty.path();
        const Result r = run_with(s, {"--check"});
        CHECK(r.code == 2);
        expect_lines(lines_of(r.out), first25, "the output before the refusal");
        CHECK(starts_with(r.err, "missing input: cannot read "));
        CHECK(r.err.find((empty.path() / mr::kSlrfRelative).string()) != std::string::npos);
        CHECK(ends_with(r.err, "\n"));
        CHECK(std::count(r.err.begin(), r.err.end(), '\n') == 1);
        // and without --check, the sizing's 22 lines (those of the record that the flags allow)
        const Result plain = run_with(s, {});
        CHECK(plain.code == 2);
        CHECK(lines_of(plain.out).size() == 22);
    }
    {   // the eccentricity file missing: the second file read
        const Tree t = worked_tree();
        fs::remove(t.root / mr::kEccRelative);
        const Result r = run_with(t.settings(), {"--check"});
        CHECK(r.code == 2);
        CHECK(starts_with(r.err, "missing input: cannot read "));
        CHECK(r.err.find(mr::kEccRelative) != std::string::npos);
        CHECK(lines_of(r.out).size() == 25);
    }
    {   // a file that is not well-formed UTF-8
        const Tree t = worked_tree();
        write_text(t.root / mr::kSlrfRelative, std::string("%=SNX 2.02 ILRS\n\xFF\xFE\n"));
        const Result r = run_with(t.settings(), {});
        CHECK(r.code == 2);
        CHECK(r.err == "missing input: " + (t.root / mr::kSlrfRelative).string() + " is not well-formed UTF-8\n");
    }
    {   // a directory in its place
        const Tree t = worked_tree();
        fs::remove(t.root / mr::kSlrfRelative);
        fs::create_directories(t.root / mr::kSlrfRelative);
        const Result r = run_with(t.settings(), {});
        CHECK(r.code == 2);
        CHECK(starts_with(r.err, "missing input: cannot read "));
        CHECK(r.err.find("it is not a regular file") != std::string::npos);
    }
    {   // a station the registry cannot read: a missing estimate (the file's own refusal passes through)
        const Tree t(slrf_text("0.6378137E+07", "0.0", "0.0", "0.0", "0.0", "0.0"), ecc_text(ecc_row("00:000:00000", "1.0", "2.0", "3.0", "70900513")));
        std::string slrf = read_text(t.root / mr::kSlrfRelative);
        slrf.replace(slrf.find("VELZ   7090"), 4, "XXXX");
        write_text(t.root / mr::kSlrfRelative, slrf);
        const Result r = run_with(t.settings(), {});
        CHECK(r.code == 2);
        CHECK(r.err == "missing input: SLRF2020 holds no VELZ estimate for 7090 A 1\n");
    }
}

TEST_CASE("the command line: --scan, --check, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[measmod_fd_sizing][behaviour]") {
    const char usage[] = "usage: measmod_fd_sizing [-h] [--scan] [--check] [--root ROOT]\n";
    const std::string help = std::string(usage) +
                             "\n"
                             "The finite-difference gate's sizing, SPEC-measmod.md section 6.2, as the numbers it freezes: the frozen sizing, the amended range sizing, the angle sizing and the diurnal-aberration check.\n"
                             "\n"
                             "options:\n"
                             "  -h, --help   show this help and exit\n"
                             "  --scan       the exact series scan of the angle third derivatives and the stencil bounds of the amended range sizing\n"
                             "  --check      exit 1 unless every number SPEC-measmod.md section 6.2 freezes is reproduced\n"
                             "  --root ROOT  the tree whose data/cache holds the station files (default: the tree this tool was built from)\n"
                             "\n"
                             "exit codes: 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced\n"
                             "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";
    TempDir empty("odl-fd");
    fd::Settings s = fd::default_settings();
    s.root = empty.path();   // a root with nothing in it: every run that gets that far is refused, quickly
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_with(s, {flag});
        CHECK(r.code == 0);
        CHECK(r.out == help);
        CHECK(r.err.empty());
    }
    CHECK(run_with(s, {"-h", "--frobnicate"}).code == 0);
    CHECK(run_with(s, {"--frobnicate", "-h"}).code == 2);
    const Result unknown = run_with(s, {"--frobnicate"});
    CHECK(unknown.code == 2);
    CHECK(unknown.err == std::string(usage) + "measmod_fd_sizing: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    CHECK(run_with(s, {"stray"}).code == 2);
    CHECK(run_with(s, {"--scan=1"}).code == 2);
    CHECK(run_with(s, {"--check=1"}).code == 2);
    CHECK(run_with(s, {"--Scan"}).code == 2);
    CHECK(run_with(s, {"-s"}).code == 2);
    const Result bare = run_with(s, {"--root"});
    CHECK(bare.code == 2);
    CHECK(bare.err == std::string(usage) + "measmod_fd_sizing: error: argument --root: expected one argument\n");
    // an argument that looks like an option is not the value of --root: the refusal says so (a root taken from it would be refused as a missing file, in other words)
    for (const char* option_like : {"--check", "---x", "--other"}) {
        const Result r = run_with(s, {"--root", option_like});
        CHECK(r.code == 2);
        CHECK(r.err == std::string(usage) + "measmod_fd_sizing: error: argument --root: expected one argument\n");
        CHECK(r.out.empty());
    }
    // --root DIR and --root=DIR name the tree: the refusal names its file (the settings' own root is replaced by the argument's, the last of two winning)
    TempDir other("odl-fd");
    for (const std::vector<std::string>& args : {std::vector<std::string>{"--root", other.path().string()}, std::vector<std::string>{"--root=" + other.path().string()},
                                                 std::vector<std::string>{"--root", empty.path().string(), "--root", other.path().string()}}) {
        const Result r = run_with(s, args);
        CHECK(r.code == 2);
        CHECK(starts_with(r.err, "missing input: cannot read " + (other.path() / mr::kSlrfRelative).string()));   // (the root as given, whole: not a piece of it)
        CHECK(r.err.find(empty.path().string()) == std::string::npos);
    }
    // the flags may come in any order and together, and the default settings' root is the tree the tool was built from
    CHECK(run_with(s, {"--check", "--scan"}).code == 2);
    CHECK(fd::default_settings().root == fd::default_root());
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[measmod_fd_sizing][behaviour]") {
    ThrowingStream out;
    std::ostringstream err;
    CHECK(fd::run_on(fd::default_settings(), {"--check"}, Streams{out, err}) == 70);
    CHECK(err.str() == "measmod_fd_sizing: internal error: boom\n");
}

// =====================================================================================================================================================================================================
// EVERY CHECK SHOWN FIRING
// =====================================================================================================================================================================================================
// The figures SPEC-measmod.md 6.2 freezes are fields of Settings::frozen (the defaults are the Python's literals).  Each check of `--check` is shown to fire at ITS OWN tolerance: the figure it compares with is damaged
// by a little less than the tolerance (the check must still pass) and a little more (it must fire), on each side, and the verdict of its own line, the number of lines that fire, the exit status and the count in
// the last line are read.  The tolerances are the Python's (measmod_fd_sizing.py: 0.005, 1.0, 1e-12, 0.01, 0.02, 0.002, 1e-6, 1e-4, 1e-3, 1e-9 and 0.6 / 2e-3 q).  The figures printed by the tool are the Python's
// literals to four or five digits, so the computed value differs from the frozen one by 1e-4 (relative) at most; the damage is chosen with that margin.

namespace {

// a line of a check that failed: the verdicts of the Python's two spellings ("NOT REPRODUCED", "NOT AGREED", ": NO", "geometry: NO)")
bool failed(const std::string& line) { return ends_with(line, "NOT REPRODUCED") || ends_with(line, "NOT AGREED") || ends_with(line, ": NO") || ends_with(line, "geometry: NO)"); }

const std::string& nth_line(const std::vector<std::string>& lines, const std::string& prefix, std::size_t nth) {
    static const std::string none = "(no such line)";
    std::size_t seen = 0;
    for (const std::string& line : lines) {
        if (starts_with(line, prefix) && seen++ == nth) return line;
    }
    return none;
}

// Damage one frozen figure by `change`, run on the real tree with `args`, and read the check whose line begins `prefix` (the nth line of that beginning): it must have failed exactly when `expect_fire`, and the number of lines
// that failed must be `expect_bad` (given as -1: 1 when it fired, else 0); the last line and the exit status say the same.
void probe_with(const std::vector<std::string>& args, const std::string& label, const std::function<void(fd::Frozen&)>& change, const std::string& prefix, std::size_t nth, bool expect_fire, int expect_bad = -1) {
    INFO(label);
    fd::Settings s = fd::default_settings();
    change(s.frozen);
    const Result r = run_with(s, args);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE_FALSE(lines.empty());
    const std::string& line = nth_line(lines, prefix, nth);
    INFO(line);
    CHECK(starts_with(line, prefix));
    CHECK(failed(line) == expect_fire);
    const int bad = expect_bad >= 0 ? expect_bad : (expect_fire ? 1 : 0);
    CHECK(static_cast<int>(std::count_if(lines.begin(), lines.end(), failed)) == bad);
    CHECK(r.code == (bad > 0 ? 1 : 0));
    CHECK(lines.back() == (bad > 0 ? "FAILED   " + std::to_string(bad) + " frozen number(s) not reproduced" : std::string("ok       every frozen number reproduced")));
    CHECK(r.err.empty());
}

void probe(const std::string& label, const std::function<void(fd::Frozen&)>& change, const std::string& prefix, std::size_t nth, bool expect_fire, int expect_bad = -1) {
    probe_with({"--check"}, label, change, prefix, nth, expect_fire, expect_bad);
}

void probe_scan(const std::string& label, const std::function<void(fd::Frozen&)>& change, const std::string& prefix, std::size_t nth, bool expect_fire, int expect_bad = -1) {
    probe_with({"--scan", "--check"}, label, change, prefix, nth, expect_fire, expect_bad);
}

// the figure is multiplied by (1 + d): d a little under the tolerance passes, a little over fires
void scaled_probes(const std::string& label, const std::function<double&(fd::Frozen&)>& field, double pass, double fire, const std::string& prefix, std::size_t nth) {
    for (const double sign : {1.0, -1.0}) {
        probe(label + " x (1 + " + std::to_string(sign * pass) + ")", [&](fd::Frozen& f) { field(f) *= 1 + sign * pass; }, prefix, nth, false);
        probe(label + " x (1 + " + std::to_string(sign * fire) + ")", [&](fd::Frozen& f) { field(f) *= 1 + sign * fire; }, prefix, nth, true);
    }
}

double& amended_field(fd::Frozen& f, std::size_t c, int which, std::size_t s) {
    fd::FrozenAmended& a = f.amended[c];
    switch (which) {
        case 0: return a.eps_rigid[s];
        case 1: return a.eps_real2[s];
        case 2: return a.eps_real1[s];
        case 3: return a.nu_chain_2;
        case 4: return a.nu_chain_1;
        case 5: return a.b_rigid;
        case 6: return a.b_real2;
        default: return a.b_real1;
    }
}

}  // namespace

TEST_CASE("--check, the frozen sizing: B_pred to 0.5 % and h* to one metre, for each of the three geometries", "[measmod_fd_sizing][behaviour]") {
    const fd::Frozen base{};
    for (std::size_t i = 0; i < 3; ++i) {
        const fd::FrozenSizing& cs = base.sizing[i];
        const fd::Sizing sz = fd::sizing(cs.rho, cs.rt, cs.range);
        const double top = *std::max_element(sz.bound.begin(), sz.bound.end());
        const std::string name = cs.name;
        // the ratio of the computed B_pred to the frozen one is 1 + d (the computed one is 4.42364e-9 for 4.42e-9 frozen, and so on: the margin to the tolerance is made by the damage, not by that)
        for (const double d : {0.0049, -0.0049}) probe("B_pred of " + name + ", ratio 1 + " + std::to_string(d), [&](fd::Frozen& f) { f.sizing[i].b_pred = top / (1 + d); }, "    frozen B_pred ", i, false);
        for (const double d : {0.0051, -0.0051}) probe("B_pred of " + name + ", ratio 1 + " + std::to_string(d), [&](fd::Frozen& f) { f.sizing[i].b_pred = top / (1 + d); }, "    frozen B_pred ", i, true);
        for (const double dh : {0.99, -0.99}) probe("h* of " + name + " off by " + std::to_string(dh), [&](fd::Frozen& f) { f.sizing[i].h_star = sz.best_step + dh; }, "    frozen B_pred ", i, false);
        for (const double dh : {1.01, -1.01}) probe("h* of " + name + " off by " + std::to_string(dh), [&](fd::Frozen& f) { f.sizing[i].h_star = sz.best_step + dh; }, "    frozen B_pred ", i, true);
    }
}

TEST_CASE("--check, the amended section's two opening checks: the IERS FCUL_A case to 1e-12 and the chain's ulp to 1 %", "[measmod_fd_sizing][behaviour]") {
    // the Decimal FCULa is V0 = 3.800243667312344087 - 2.37e-16 = 3.80024366731234385 (R1 line 31); with the printed value V0 - d the difference is d
    const std::string iers = "the Decimal FCULa at the IERS FCUL_A printed case differs from the printed ";
    struct Case {
        const char* printed;
        bool fires;
    };
    for (const Case& c : {Case{"3.800243667311443850", false}, Case{"3.800243667313243850", false}, Case{"3.800243667311243850", true}, Case{"3.800243667313443850", true}}) {
        probe(std::string("IERS printed value ") + c.printed, [&](fd::Frozen& f) { f.iers_fcula = c.printed; }, iers, 0, c.fires);
    }
    // the chain's ulp: the ratio of one ulp of the Earth-rotation angle at the station to the predicted 1.59e-7 is 1 + d
    Sixty sixty;
    const double floor_one = fd::atmosphere_inputs(mr::default_root()).r_perp * std::ldexp(1.0, -45);
    const std::string ulp = "one ulp of the chain's angle at the station = ";
    for (const double d : {0.0099, -0.0099}) probe("chain ulp, ratio 1 + " + std::to_string(d), [&](fd::Frozen& f) { f.chain_ulp = floor_one / (1 + d); }, ulp, 0, false);
    for (const double d : {0.0101, -0.0101}) probe("chain ulp, ratio 1 + " + std::to_string(d), [&](fd::Frozen& f) { f.chain_ulp = floor_one / (1 + d); }, ulp, 0, true);
}

TEST_CASE("--check, the amended section: eps(h) of the three configurations, nu_chain and B_pred of each case to 0.2 %", "[measmod_fd_sizing][behaviour]") {
    const std::string prefix = "    the frozen amended eps(h), nu_chain and B_pred reproduced: ";
    const char* names[8] = {"eps_rigid", "eps_real2", "eps_real1", "nu_chain_2", "nu_chain_1", "b_rigid", "b_real2", "b_real1"};
    for (std::size_t c = 0; c < 2; ++c) {
        for (int which = 0; which < 8; ++which) {
            const std::size_t sizes = which < 3 ? 5 : 1;
            for (std::size_t s = 0; s < sizes; ++s) {
                scaled_probes(std::string(names[which]) + "[" + std::to_string(s) + "] of case " + std::to_string(c), [&](fd::Frozen& f) -> double& { return amended_field(f, c, which, s); }, 0.0018, 0.0022, prefix, c);
            }
        }
    }
}

TEST_CASE("--check, the angle section: the published and the frozen numbers, each at its tolerance", "[measmod_fd_sizing][behaviour]") {
    // eraRefco against ERFA's published case, in units of its tolerances (1e-15 for A, 1e-18 for B): the check is d < 1
    const std::string erfa = "the 60-digit eraRefco against ERFA's published case: ";
    for (const double sign : {1.0, -1.0}) {
        probe("ERFA's A off by 0.9 tolerance", [&](fd::Frozen& f) { f.erfa_a += sign * 0.9e-15; }, erfa, 0, false);
        probe("ERFA's A off by 1.1 tolerance", [&](fd::Frozen& f) { f.erfa_a += sign * 1.1e-15; }, erfa, 0, true);
        probe("ERFA's B off by 0.9 tolerance", [&](fd::Frozen& f) { f.erfa_b += sign * 0.9e-18; }, erfa, 0, false);
        probe("ERFA's B off by 1.1 tolerance", [&](fd::Frozen& f) { f.erfa_b += sign * 1.1e-18; }, erfa, 0, true);
    }
    // A and B of the first normal point's weather to 1e-6 (the frozen digits are rounded to 7: the computed values differ by 2e-7 at most)
    const std::string ab = "the frozen A and B of the first normal point's weather reproduced: ";
    scaled_probes("A", [](fd::Frozen& f) -> double& { return f.angle_refraction.first; }, 0.7e-6, 1.3e-6, ab, 0);
    scaled_probes("B", [](fd::Frozen& f) -> double& { return f.angle_refraction.second; }, 0.7e-6, 1.3e-6, ab, 0);
    // nu_a to 1e-4
    scaled_probes("nu_a", [](fd::Frozen& f) -> double& { return f.angle_nu_a; }, 0.8e-4, 1.2e-4, "the frozen nu_a reproduced: ", 0);
    // eps(h) at the five sizes and B_pred of each of the four groups to 1e-3
    const std::string groups = "the frozen eps(h) and B_pred of the four groups reproduced from F_pure and the frozen F_extra: ";
    for (std::size_t g = 0; g < 4; ++g) {
        for (std::size_t i = 0; i < 5; ++i) {
            scaled_probes("eps[" + std::to_string(i) + "] of group " + std::to_string(g), [&](fd::Frozen& f) -> double& { return f.angle_groups[g].eps[i]; }, 0.0008, 0.0012, groups, 0);
        }
        scaled_probes("B_pred of group " + std::to_string(g), [&](fd::Frozen& f) -> double& { return f.angle_groups[g].b_pred; }, 0.0008, 0.0012, groups, 0);
    }
    // the agreement tolerances to 1e-3
    for (std::size_t i = 0; i < 5; ++i) {
        scaled_probes("agreement[" + std::to_string(i) + "]", [&](fd::Frozen& f) -> double& { return f.angle_agreement[i]; }, 0.0008, 0.0012, "the frozen agreement tolerances reproduced: ", 0);
    }
}

TEST_CASE("--check, the angle section: F_extra is 1.05 x the fine scan rounded up to three digits", "[measmod_fd_sizing][behaviour]") {
    const std::string prefix = "the frozen F_extra are 1.05 x the fine scan, rounded up to three digits: ";
    // 1.05 x the fine scan over the unit of the third digit: 261.99 (geometric: 2.62e-4), 142.60 (astrometric: 1.43e-3), 973.54 (refracted: 9.74e-2): the round-up stays while the multiple stays in (261, 262], (142, 143], (973, 974]
    struct Case {
        std::size_t group;
        double pass_up, pass_down, fire_up, fire_down;
    };
    for (const Case& c : {Case{0, 0.9999, 0.998, 1.0001, 0.99}, Case{1, 1.002, 0.9965, 1.0035, 0.99}, Case{2, 1.0003, 0.9996, 1.0007, 0.999}}) {
        for (const double m : {c.pass_up, c.pass_down}) probe("fine of group " + std::to_string(c.group) + " x " + std::to_string(m), [&](fd::Frozen& f) { f.angle_groups[c.group].fine *= m; }, prefix, 0, false);
        for (const double m : {c.fire_up, c.fire_down}) probe("fine of group " + std::to_string(c.group) + " x " + std::to_string(m), [&](fd::Frozen& f) { f.angle_groups[c.group].fine *= m; }, prefix, 0, true);
        // and the frozen F_extra itself, to the 1e-9 the comparison allows: 5e-10 passes, 2e-9 fires
        probe("f_extra of group " + std::to_string(c.group) + " x (1 + 5e-10)", [&](fd::Frozen& f) { f.angle_groups[c.group].f_extra *= 1 + 5e-10; }, prefix, 0, false);
        probe("f_extra of group " + std::to_string(c.group) + " x (1 + 2e-9)", [&](fd::Frozen& f) { f.angle_groups[c.group].f_extra *= 1 + 2e-9; }, prefix, 0, true);
        probe("f_extra of group " + std::to_string(c.group) + " x (1 - 5e-10)", [&](fd::Frozen& f) { f.angle_groups[c.group].f_extra *= 1 - 5e-10; }, prefix, 0, false);
        probe("f_extra of group " + std::to_string(c.group) + " x (1 - 2e-9)", [&](fd::Frozen& f) { f.angle_groups[c.group].f_extra *= 1 - 2e-9; }, prefix, 0, true);
    }
    // the fourth group (of date) has no fine scan of its own: whatever its `fine` says is not compared
    probe("fine of the group of date", [](fd::Frozen& f) { f.angle_groups[3].fine = 1.234e-3; }, prefix, 0, false);
    probe("fine of the group of date, zero", [](fd::Frozen& f) { f.angle_groups[3].fine = 0.0; }, prefix, 0, false);
}

TEST_CASE("--check, the diurnal section: nu, eps(h), B_pred, F_extra, the two row differences and the predicted powers, each at its tolerance", "[measmod_fd_sizing][behaviour]") {
    // nu to 1e-4
    scaled_probes("nu", [](fd::Frozen& f) -> double& { return f.diurnal.nu; }, 0.8e-4, 1.2e-4, "the frozen nu of this geometry reproduced: ", 0);
    // eps(h) at the five sizes and B_pred to 1e-3
    const std::string eps = "the frozen eps(h) and B_pred of this geometry reproduced: ";
    for (std::size_t i = 0; i < 5; ++i) scaled_probes("eps[" + std::to_string(i) + "]", [&](fd::Frozen& f) -> double& { return f.diurnal.eps[i]; }, 0.0008, 0.0012, eps, 0);
    scaled_probes("B_pred", [](fd::Frozen& f) -> double& { return f.diurnal.b_pred; }, 0.0008, 0.0012, eps, 0);
    // F_extra: 1.05 x the fine scan rounded up to three digits.  1.05 x 2.0815e-2 = 0.02185575: 218.56 units of 1e-4, rounded up to 219.
    const std::string rounding = "the frozen F_extra is 1.05 x the fine scan, rounded up to three digits: ";
    probe("fine_extra 2.08e-2", [](fd::Frozen& f) { f.diurnal.fine_extra = 2.08e-2; }, rounding, 0, false);   // 218.4: still 219
    probe("fine_extra 2.1e-2", [](fd::Frozen& f) { f.diurnal.fine_extra = 2.1e-2; }, rounding, 0, true);       // 220.5: 221
    probe("fine_extra 2.07e-2", [](fd::Frozen& f) { f.diurnal.fine_extra = 2.07e-2; }, rounding, 0, true);     // 217.35: 218
    // the frozen F_extra itself: the comparison is exact equality or a relative 1e-9
    probe("f_extra x (1 + 5e-10)", [](fd::Frozen& f) { f.diurnal.f_extra *= 1 + 5e-10; }, rounding, 0, false);
    probe("f_extra x (1 - 5e-10)", [](fd::Frozen& f) { f.diurnal.f_extra *= 1 - 5e-10; }, rounding, 0, false);
    probe("f_extra x (1 + 2e-9)", [](fd::Frozen& f) { f.diurnal.f_extra *= 1 + 2e-9; }, rounding, 0, true);
    probe("f_extra x (1 - 2e-9)", [](fd::Frozen& f) { f.diurnal.f_extra *= 1 - 2e-9; }, rounding, 0, true);
    // the two row differences to 1e-3
    const std::string rows = "the frozen row differences (without A', A' reversed) reproduced: ";
    scaled_probes("delta_without", [](fd::Frozen& f) -> double& { return f.diurnal.delta_without; }, 0.0008, 0.0012, rows, 0);
    scaled_probes("delta_reversed", [](fd::Frozen& f) -> double& { return f.diurnal.delta_reversed; }, 0.0008, 0.0012, rows, 0);
    // the predicted powers, whole numbers of eps: they agree to the rounding (0.6) or to 0.2 %, whichever is larger; the frozen whole numbers are the computed ones rounded, so the computed differs by 0.5 at most
    const std::string powers = "the frozen predicted powers of the two controls (in eps, at the five sizes) reproduced: ";
    for (const int which : {0, 1}) {
        for (std::size_t i = 0; i < 5; ++i) {
            const fd::Frozen base{};
            const double q0 = (which == 0 ? base.diurnal.power_without : base.diurnal.power_reversed)[i];
            const double allowed = std::max(0.6, 2e-3 * q0);
            const double pass = allowed - 0.55;          // the computed value is within 0.5 of q0: at most `allowed - 0.05` from the damaged figure
            const double fire = allowed * 1.01 + 0.6;    // and at least `1.01 allowed + 0.1` from it
            for (const double sign : {1.0, -1.0}) {
                const auto set = [&](double t) {
                    return [=](fd::Frozen& f) { (which == 0 ? f.diurnal.power_without : f.diurnal.power_reversed)[i] = q0 + sign * t; };
                };
                const std::string label = std::string(which == 0 ? "power_without[" : "power_reversed[") + std::to_string(i) + "] " + (sign > 0 ? "+ " : "- ");
                probe(label + std::to_string(pass), set(pass), powers, 0, false);
                probe(label + std::to_string(fire), set(fire), powers, 0, true);
            }
        }
    }
}

TEST_CASE("--scan --check, the checks that need the scan: F_trop to 2 %, the angle scan against F_extra and the fine scan, the diurnal fine scan to 1e-3", "[measmod_fd_sizing][real_tree]") {
    // F_trop of the LEO-like case at h = 1000 m: the scan's value against the frozen literal, 2 %
    const std::string trop = "    frozen F_trop reproduced: ";
    probe_scan("F_trop of the LEO-like case x 1.019", [](fd::Frozen& f) { f.amended[1].f_trop[4] *= 1.019; }, trop, 1, false);
    probe_scan("F_trop of the LEO-like case x 1.0215", [](fd::Frozen& f) { f.amended[1].f_trop[4] *= 1.0215; }, trop, 1, true);
    probe_scan("F_trop of the LAGEOS-like case x 0.981", [](fd::Frozen& f) { f.amended[0].f_trop[0] *= 0.981; }, trop, 0, false);
    probe_scan("F_trop of the LAGEOS-like case x 0.9795", [](fd::Frozen& f) { f.amended[0].f_trop[0] *= 0.9795; }, trop, 0, true);
    // the angle scan of the geometric group: sup |full - pure| = 2.4219e-04 must be below the frozen F_extra and above 85 % of the fine scan.  Damaged, the rounding of F_extra from the fine scan fires with it.
    const std::string below = "    below the frozen F_extra ";
    probe_scan("F_extra of the geometric group 2.40e-4, below the scan's 2.4219e-4", [](fd::Frozen& f) { f.angle_groups[0].f_extra = 2.40e-4; }, below, 0, true, 2);
    probe_scan("fine scan of the geometric group 2.9e-4, whose 85 % is above the scan's 2.4219e-4", [](fd::Frozen& f) { f.angle_groups[0].fine = 2.9e-4; }, below, 0, true, 2);
    probe_scan("fine scan of the geometric group 2.8e-4, whose 85 % is 2.38e-4, below the scan's", [](fd::Frozen& f) { f.angle_groups[0].fine = 2.8e-4; }, below, 0, false, 1);
    // the diurnal fine scan, to 1e-3: the scan's 2.0815e-02
    const std::string fine = "    the frozen fine scan ";
    probe_scan("diurnal fine scan x 1.0008", [](fd::Frozen& f) { f.diurnal.fine_extra *= 1.0008; }, fine, 0, false);
    probe_scan("diurnal fine scan x 1.0012", [](fd::Frozen& f) { f.diurnal.fine_extra *= 1.0012; }, fine, 0, true);
    probe_scan("diurnal fine scan x 0.9988", [](fd::Frozen& f) { f.diurnal.fine_extra *= 0.9988; }, fine, 0, true);
    // the third check of a group: the full third derivative stays below F_a, the largest of F_pure rho^3 + F_extra over the five sizes.  A F_extra of -1 puts that bound (3.4) below the scan's supremum, and it takes with it
    // the frozen eps(h) and B_pred, the rounding of F_extra and the check below F_extra: four lines fire, in each the count of the last line
    const std::string full = "    the full third derivative stays below F_a (the largest of the five sizes, ";
    probe_scan("F_extra of the geometric group -1", [](fd::Frozen& f) { f.angle_groups[0].f_extra = -1.0; }, full, 0, true, 4);
    probe_scan("the frozen F_extra of the geometric group as it is: the check holds", [](fd::Frozen&) {}, full, 0, false, 0);
}

// =====================================================================================================================================================================================================
// THE SCANS WRITTEN OUT AGAIN
// =====================================================================================================================================================================================================
// The loops of the scans fix grids whose every cell the printed output (four to six digits) hardly sees: a cell moved, dropped or doubled changes the supremum by a few units of the last place, or not at all.  Each scan is
// therefore written out AGAIN here, from the text of the Python's own function (measmod_fd_sizing.py: scan, trop_shapiro_scan, coupling_scan), with its own loops, constants and order of arithmetic, and the two are required
// to give the SAME DOUBLES, bit for bit.  The C library's functions are the devkit's wrappers (py_sin, py_cos, py_log1p, py_pow), which the compiler cannot merge (GCC turns a sin and a cos of one argument into one sincos),
// so the comparison does not depend on a platform's rounding.

namespace {

constexpr double kDegrees = std::numbers::pi / 180;   // math.radians(x) is x * (pi / 180)

double norm_again(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

// scan(): the declinations -30 .. 30 in steps of 10 degrees, 12 right ascensions 30 degrees apart from 0.3 rad (those with x > 0 kept), 90 polar angles at the middle of 2 degree cells, 180 azimuths
std::array<double, 2> scan_again() {
    std::array<double, 2> best = {0.0, 0.0};
    for (const int dec_deg : {-30, -20, -10, 0, 10, 20, 30}) {
        const double dec = dec_deg * kDegrees;
        for (int k = 0; k < 12; ++k) {
            const double ra = 2 * std::numbers::pi * k / 12 + 0.3;
            const std::array<double, 3> g = {py_cos(dec) * py_cos(ra), py_cos(dec) * py_sin(ra), py_sin(dec)};
            if (g[0] <= 0) continue;
            for (int it = 0; it < 90; ++it) {
                const double th = std::numbers::pi * (it + 0.5) / 90;
                for (int jt = 0; jt < 180; ++jt) {
                    const double ph = 2 * std::numbers::pi * jt / 180;
                    const std::array<double, 3> e = {py_sin(th) * py_cos(ph), py_sin(th) * py_sin(ph), py_cos(th)};
                    const std::array<double, 2> d = fd::third_derivatives(g, e);
                    best[0] = best[0] < std::fabs(d[0]) ? std::fabs(d[0]) : best[0];
                    best[1] = best[1] < std::fabs(d[1]) ? std::fabs(d[1]) : best[1];
                }
            }
        }
    }
    return best;
}

// trop_shapiro_scan(): the zenith delay times the mapping function plus the Shapiro term, third differences along every direction of a (theta, phi) grid, at the stencil points +-xi of the sizes
fd::TropScan trop_again(double rho, double el_deg, const fd::Atmosphere& atm, double step_theta, double step_phi, double delta) {
    const double cphi = py_cos(atm.lat);
    const double el = el_deg * kDegrees;
    const std::array<double, 3> p0 = {rho * py_cos(el), 0.0, rho * py_sin(el)};
    const double r_station = 6.373e6;
    const double k_shapiro = 2.0 * 3.986004415e14 / py_pow(fd::kLight, 2);
    const auto total = [&](double px, double py, double pz, bool with_atm, bool with_shapiro) {
        const double r = norm_again(px, py, pz);
        double out = 0.0;
        if (with_atm) out += atm.z * fd::fcula(pz / r, cphi, atm.height, atm.t_c);
        if (with_shapiro) {
            const double r2 = norm_again(px, py, pz + r_station);
            out += k_shapiro * py_log1p(2 * r / (r_station + r2 - r));
        }
        return out;
    };
    const auto third = [&](const std::array<double, 3>& e, double xi, bool with_atm, bool with_shapiro) {
        std::array<double, 4> f{};
        std::size_t index = 0;
        for (const int k : {-2, -1, 1, 2}) {
            const double t = xi + k * delta;
            f[index++] = total(p0[0] + t * e[0], p0[1] + t * e[1], p0[2] + t * e[2], with_atm, with_shapiro);
        }
        return (f[3] - 2 * f[2] + 2 * f[1] - f[0]) / (2 * py_pow(delta, 3));
    };
    fd::TropScan out{};
    const int n_th = static_cast<int>(180 / step_theta) + 1;
    const int n_ph = static_cast<int>(360 / step_phi);
    const auto larger = [](double a, double b) { return b > a ? b : a; };
    for (int it = 0; it < n_th; ++it) {
        const double th = step_theta * it * kDegrees;
        for (int jp = 0; jp < n_ph; ++jp) {
            const double ph = step_phi * jp * kDegrees;
            const std::array<double, 3> e = {py_sin(th) * py_cos(ph), py_sin(th) * py_sin(ph), py_cos(th)};
            for (const double sign : {1.0, -1.0}) {
                for (const double xi_abs : {0.0, 10.0, 30.0, 100.0, 300.0, 1000.0}) {
                    const double xi = sign * xi_abs;
                    const double v = std::fabs(third(e, xi, true, true));
                    for (std::size_t s = 0; s < 5; ++s) {
                        if (xi_abs <= fd::kSteps[s]) out.sup[s] = larger(out.sup[s], v);
                    }
                    if (xi_abs == 1000.0 || xi_abs == 0.0) {
                        out.sup_shapiro = larger(out.sup_shapiro, std::fabs(third(e, xi, false, true)));
                        out.sup_atmosphere = larger(out.sup_atmosphere, std::fabs(third(e, xi, true, false)));
                    }
                }
            }
        }
    }
    return out;
}

// coupling_scan(): the closed-form two-way range of a target in uniform motion (60 digits), third differences along the cone of directions (mu = e.g in 0.45 .. 0.70, 24 azimuths of the cone) at the stencil points
std::array<double, 5> coupling_again(double rho, double el_deg, double az_deg, double speed, double radial, int event, double delta) {
    const double el = el_deg * kDegrees;
    const double az = az_deg * kDegrees;
    const std::array<double, 3> g = {py_cos(el) * py_cos(az), py_cos(el) * py_sin(az), py_sin(el)};
    const std::array<double, 3> up = {0.0, 0.0, 1.0};
    std::array<double, 3> across = {g[1] * up[2] - g[2] * up[1], g[2] * up[0] - g[0] * up[2], g[0] * up[1] - g[1] * up[0]};
    const double n = norm_again(across[0], across[1], across[2]);
    for (double& x : across) x = x / n;
    std::array<double, 3> vt{};
    for (std::size_t i = 0; i < 3; ++i) vt[i] = speed * (std::sqrt(1.0 - radial * radial) * across[i] + radial * g[i]);
    const std::array<double, 3> s0 = {0.0, 0.0, 0.0};
    const std::array<double, 3> vs = {fd::v_station_max(), 0.0, 0.0};
    mr::Vec r0;
    for (std::size_t i = 0; i < 3; ++i) r0[i] = Decimal::from_double(rho * g[i]);
    std::array<double, 3> a_ax{};
    for (std::size_t i = 0; i < 3; ++i) a_ax[i] = up[i] - g[2] * g[i];
    const double na = norm_again(a_ax[0], a_ax[1], a_ax[2]);
    for (double& x : a_ax) x = x / na;
    const std::array<double, 3> b_ax = {g[1] * a_ax[2] - g[2] * a_ax[1], g[2] * a_ax[0] - g[0] * a_ax[2], g[0] * a_ax[1] - g[1] * a_ax[0]};
    const Decimal c(299792458);
    const Decimal dl = Decimal::from_double(delta);
    const auto to_vec = [](const std::array<double, 3>& v) { return mr::Vec{Decimal::from_double(v[0]), Decimal::from_double(v[1]), Decimal::from_double(v[2])}; };
    const mr::Vec s0d = to_vec(s0);
    const mr::Vec vsd = to_vec(vs);
    const mr::Vec vtd = to_vec(vt);
    const auto y = [&](const mr::Vec& r) {
        const auto [tu, td] = mr::lt_closed_form(event, s0d, vsd, r, vtd);
        return c * (tu + td) / Decimal(2);
    };
    std::array<double, 5> sup{};
    for (const double mu : {0.45, 0.50, 0.55, 1 / std::sqrt(3.0), 0.60, 0.65, 0.70}) {
        const double sn = std::sqrt(1 - mu * mu);
        for (int k = 0; k < 24; ++k) {
            const double ph = 15.0 * k * kDegrees;
            std::array<double, 3> e{};
            for (std::size_t i = 0; i < 3; ++i) e[i] = mu * g[i] + sn * (py_cos(ph) * a_ax[i] + py_sin(ph) * b_ax[i]);
            const mr::Vec ed = to_vec(e);
            for (const double sign : {1.0, -1.0}) {
                for (const double xi_abs : {0.0, 10.0, 30.0, 100.0, 300.0, 1000.0}) {
                    const Decimal xi = Decimal::from_double(sign * xi_abs);
                    std::array<Decimal, 4> ys;
                    std::size_t index = 0;
                    for (const int kk : {-2, -1, 1, 2}) {
                        const Decimal t = xi + Decimal(kk) * dl;
                        ys[index++] = y({r0[0] + t * ed[0], r0[1] + t * ed[1], r0[2] + t * ed[2]});
                    }
                    const double d3 = std::fabs(((ys[3] - Decimal(2) * ys[2] + Decimal(2) * ys[1] - ys[0]) / (Decimal(2) * dl.pow(Decimal(3)))).to_double());
                    for (std::size_t s = 0; s < 5; ++s) {
                        if (xi_abs <= fd::kSteps[s]) sup[s] = d3 > sup[s] ? d3 : sup[s];
                    }
                }
            }
        }
    }
    return sup;
}

}  // namespace

TEST_CASE("aberrate: A(n; beta) and D(n; beta) of MEAS-R-053 equal the reference generator's 60-digit values in its four cases (MEAS-A-080 and -081)", "[measmod_fd_sizing][behaviour]") {
    const Sixty sixty;
    const mr::Values values = mr::aberration_section();
    const auto value = [&](const std::string& name) {
        for (const auto& entry : values) {
            if (entry.first == name) return entry.second;
        }
        FAIL("no value named " << name);
        return 0.0;
    };
    for (const char* name : {"annual", "oblique", "large", "diurnal"}) {
        const std::string c = name;
        const fd::Vec3 n = {value("ab_" + c + "_n_x"), value("ab_" + c + "_n_y"), value("ab_" + c + "_n_z")};
        const fd::Vec3 beta = {value("ab_" + c + "_beta_x"), value("ab_" + c + "_beta_y"), value("ab_" + c + "_beta_z")};
        const fd::SVec3 series = {fd::Series(n[0]), fd::Series(n[1]), fd::Series(n[2])};
        const fd::SVec3 a = fd::aberrate(series, beta);
        const fd::SVec3 d = fd::aberrate(series, fd::Vec3{-beta[0], -beta[1], -beta[2]});
        for (std::size_t i = 0; i < 3; ++i) {
            const std::string x(1, "xyz"[i]);
            INFO(c << " " << x);
            CHECK(within(a[i].c[0], value("ab_" + c + "_a_" + x), 4e-16, "A"));
            CHECK(within(d[i].c[0], value("ab_" + c + "_d_" + x), 4e-16, "D"));
        }
    }
}

TEST_CASE("az_series_any: arctan(y / x) for |x| >= |y|, else +-pi/2 - arctan(x / y), with the derivative of the azimuth, on both sides of the singularity", "[measmod_fd_sizing][behaviour]") {
    using S = fd::Series;
    const double pi = std::numbers::pi;
    // the direction (x, y, z) displaced by s (ex, ey, ez): the azimuth atan2(y, x) up to the multiple of pi the first branch leaves, and d/ds = (x ey - y ex) / (x^2 + y^2) in either
    const auto az = [](double x, double y, double ex, double ey) { return fd::az_series_any(fd::SVec3{S(x, ex), S(y, ey), S(1.0, 0.0)}); };
    const auto derivative = [](double x, double y, double ex, double ey) { return (x * ey - y * ex) / (x * x + y * y); };
    struct Case {
        double x, y, ex, ey, value;
    };
    const double third = std::atan(1.0 / 3.0);
    for (const Case& c : {Case{0.9, 0.3, 0.1, -0.2, std::atan(0.3 * (1.0 / 0.9))},   // the first branch
                          Case{-0.9, 0.3, 0.1, 0.2, std::atan(0.3 * (1.0 / -0.9))},                // ... with x < 0: arctan(y / x), not atan2
                          Case{0.5, 0.5, 0.3, -0.1, pi / 4},                                       // |x| = |y|
                          Case{0.3, 0.9, 0.1, -0.2, pi / 2 - third},                               // the second branch, y > 0
                          Case{0.3, -0.9, 0.1, 0.2, -pi / 2 + third},                              // ... y < 0, with ey of the other sign than y
                          Case{-0.3, 0.9, 0.2, -0.1, pi / 2 + third},                              // ... x < 0
                          Case{0.0, 1.0, 0.0, 0.0, pi / 2},                                        // due east, x exactly 0 and no displacement of y: the first branch would divide by 0
                          Case{0.0, -1.0, 0.3, 0.0, -pi / 2}}) {
        INFO("x " << c.x << ", y " << c.y << ", ex " << c.ex << ", ey " << c.ey);
        const S s = az(c.x, c.y, c.ex, c.ey);
        CHECK(within(s.c[0], c.value, 2e-16, "azimuth"));
        CHECK(within(s.c[1], derivative(c.x, c.y, c.ex, c.ey), 4e-15, "d azimuth / ds"));
        for (std::size_t i = 0; i < 4; ++i) CHECK(std::isfinite(s.c[i]));
    }
}

TEST_CASE("scenario_vectors: the target's velocity across or along the line of sight, in units of c, and the acceleration toward the geocentre in rho/c^2", "[measmod_fd_sizing][behaviour]") {
    const double c = fd::kLight;
    const double speed = 7500.0 / c;
    const double a_mag = 3.986004415e14 / (7.2e6 * 7.2e6);   // m/s^2: GM / r^2 at r = 7.2e6 m (7.69)
    const double scale = -a_mag * 1.2e6 / (c * c);           // rho = 1.2e6 m: the acceleration in rho/c^2 per unit of the line of sight
    struct Case {
        fd::Vec3 los;
        double radial;
        fd::Vec3 across;   // los x up, normalised, by hand
    };
    for (const Case& k : {Case{{0.6, 0.0, 0.8}, 0.0, {0.0, -1.0, 0.0}}, Case{{0.6, 0.0, 0.8}, 1.0, {0.0, -1.0, 0.0}}, Case{{0.36, 0.48, -0.8}, 0.0, {0.8, -0.6, 0.0}},
                          Case{{0.36, 0.48, -0.8}, 0.6, {0.8, -0.6, 0.0}}, Case{{0.0, 0.6, 0.8}, 0.3, {1.0, 0.0, 0.0}}}) {
        INFO("los " << k.los[0] << " " << k.los[1] << " " << k.los[2] << ", radial " << k.radial);
        const auto [v, a] = fd::scenario_vectors(k.los, k.radial);
        const double sn = std::sqrt(1.0 - k.radial * k.radial);
        for (std::size_t i = 0; i < 3; ++i) {
            INFO("component " << i);
            CHECK(within(v[i], speed * (sn * k.across[i] + k.radial * k.los[i]), 1e-19, "v_hat"));
            CHECK(within(a[i], scale * k.los[i], 1e-24, "a_hat"));
        }
        // the speed is 7.5 km/s whatever the mixture (the two parts are perpendicular), and the acceleration is 7.7 m/s^2 (1.0266e-10 in rho/c^2)
        CHECK(within(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), speed, 1e-19, "|v_hat|"));
        CHECK(within(std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), -scale, 1e-24, "|a_hat|"));
    }
}

TEST_CASE("scan, written out again from the Python's text: the same two doubles", "[measmod_fd_sizing][behaviour]") {
    const std::array<double, 2> again = scan_again();
    const std::array<double, 2> got = fd::scan();
    CHECK(got[0] == again[0]);
    CHECK(got[1] == again[1]);
    CHECK(got[0] > 3.07);   // (not 0: the comparison above is of two numbers that were computed)
}

TEST_CASE("trop_shapiro_scan, written out again from the Python's text: the same doubles on coarse and on fine grids", "[measmod_fd_sizing][behaviour]") {
    // an atmosphere that is not the real station's, so that a defect that happens to cancel on the real one does not
    const fd::Atmosphere atm{5580552.4, -0.507, 244.5, 37.15, 2.364942};
    struct Grid {
        double rho, el, step_theta, step_phi, delta;
    };
    for (const Grid& grid : {Grid{6.6e6, 52.0, 90.0, 90.0, 2000.0}, Grid{6.6e6, 52.0, 45.0, 60.0, 2000.0}, Grid{1.5e6, 15.0, 30.0, 40.0, 500.0}, Grid{1.5e6, 15.0, 60.0, 45.0, 2000.0}, Grid{6.6e6, 20.0, 20.0, 30.0, 100.0},
                             Grid{3.0e6, 80.0, 36.0, 72.0, 1000.0}}) {
        INFO("rho " << grid.rho << ", el " << grid.el << ", steps " << grid.step_theta << " " << grid.step_phi << ", delta " << grid.delta);
        const fd::TropScan want = trop_again(grid.rho, grid.el, atm, grid.step_theta, grid.step_phi, grid.delta);
        const fd::TropScan got = fd::trop_shapiro_scan(grid.rho, grid.el, atm, grid.step_theta, grid.step_phi, grid.delta);
        CHECK(got.sup == want.sup);
        CHECK(got.sup_shapiro == want.sup_shapiro);
        CHECK(got.sup_atmosphere == want.sup_atmosphere);
        CHECK(got.sup[0] > 0.0);
        CHECK(got.sup_shapiro > 0.0);
        CHECK(got.sup_atmosphere > 0.0);
    }
    // the default grid of the real station's sections (3 degrees by 4, delta 2000 m) is the one the tool uses: the first size's supremum against the second implementation's, once
    const fd::TropScan want = trop_again(6.6e6, 52.0, atm, 3.0, 4.0, 2000.0);
    const fd::TropScan got = fd::trop_shapiro_scan(6.6e6, 52.0, atm);
    CHECK(got.sup == want.sup);
    CHECK(got.sup_shapiro == want.sup_shapiro);
    CHECK(got.sup_atmosphere == want.sup_atmosphere);
}

TEST_CASE("coupling_scan, written out again from the Python's text: the same doubles in both events, both velocity directions and two geometries", "[measmod_fd_sizing][behaviour]") {
    const Sixty sixty;
    struct Case {
        double rho, el, az, speed, radial;
        int event;
        double delta;
    };
    for (const Case& c : {Case{6.6e6, 52.0, 105.0, 5700.0, 0.0, 2, 100.0}, Case{6.6e6, 52.0, 105.0, 5700.0, 1.0, 1, 100.0}, Case{1.5e6, 15.0, 0.0, 7500.0, 1.0, 2, 100.0}, Case{1.5e6, 15.0, 0.0, 7500.0, 0.0, 1, 50.0}}) {
        INFO("rho " << c.rho << ", el " << c.el << ", az " << c.az << ", speed " << c.speed << ", radial " << c.radial << ", event " << c.event << ", delta " << c.delta);
        const std::array<double, 5> want = coupling_again(c.rho, c.el, c.az, c.speed, c.radial, c.event, c.delta);
        const std::array<double, 5> got = fd::coupling_scan(c.rho, c.el, c.az, c.speed, c.radial, c.event, c.delta);
        CHECK(got == want);
        CHECK(got[0] > 0.0);
    }
}
