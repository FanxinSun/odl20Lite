// tests/devtools/msis_reference_tests.cpp — freezing the NRLMSISE-00 reference implementation's output (plan L0 step 8, group C10): the sweep's tables and the input it feeds the Fortran, the first 2,437 lines of the
// model, the reference's output as the tool reads it, the classification of 2,172 comparisons by hand, the generated header (line by line on a small case, and rebuilt from the committed header's own data
// on the real one), `which`, the command line, and the whole tool through a stand-in compiler (tests/devtools/fake_gfortran.cpp and fake_reference.cpp) with every refusal, and the stand-in reference's own reading of standard input (byte for byte, at the block and pipe sizes).
// (ctests `msis_reference.behaviour` and `msis_reference.real_tree`; the hidden case [.regeneration] runs the real gfortran when asked for by tag.)
//
// gfortran is a regeneration-only host program (ATMO-R-028: not part of the build, the tests or CI; not declared in the manifest; not redistributed — the maintainer's ruling D9).  No case of [behaviour] or
// [real_tree] needs it, and nothing in the build may come to need it.
//
// Every expectation is DERIVED BY HAND from the statements of msis_reference.py (read from git, never run), or comes from a SECOND METHOD written here apart from the tool (a brute-force classification of the
// numbers the stand-in reference prints; a shortest-round-trip float formatter; a reader of the committed header), or from the committed header (produced by the Python on 2026-09-23) and the pinned Fortran's
// own driver; none is taken from running the port to see what it says.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/process.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/tool.hpp>

#include "msis_reference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace ms = odl::tools::msis_reference;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const ms::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = ms::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

// the format is always a literal at the call sites; the wrapper takes it as an argument
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
std::string fmt(const char* format, double v) {
    char buf[128];
    std::snprintf(buf, sizeof buf, format, v);
    return buf;
}
#pragma GCC diagnostic pop

std::string pad5(std::size_t n) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%5zu", n);
    return buf;
}
std::string pad3(long long n) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%3lld", n);
    return buf;
}

// Python's repr of a float, by the shortest decimal that reads back as the same double (for the magnitudes of the sweep: no exponent form is ever needed)
std::string py_repr(double v) {
    // Python's repr: the shortest digits that read back as the same double, in decimal notation for -4 <= exponent < 16 (every value of the sweep)
    if (v == 0.0) return std::signbit(v) ? "-0.0" : "0.0";
    char buf[64];
    int precision = 1;
    for (; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof buf, "%.*e", precision - 1, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    const std::string sci = buf;   // [-]d[.ddd]e[+-]xx
    const bool negative = sci.front() == '-';
    const std::size_t e_at = sci.find('e');
    REQUIRE(e_at != std::string::npos);
    std::string digits;
    for (std::size_t i = negative ? 1 : 0; i < e_at; ++i) {
        if (sci.at(i) != '.') digits += sci.at(i);
    }
    const int exponent = std::atoi(sci.c_str() + e_at + 1);
    REQUIRE(exponent >= -4);
    REQUIRE(exponent < 16);
    std::string out;
    if (exponent >= 0) {
        const std::size_t int_digits = static_cast<std::size_t>(exponent) + 1;
        while (digits.size() < int_digits) digits += '0';
        out = digits.substr(0, int_digits) + "." + (digits.size() > int_digits ? digits.substr(int_digits) : "0");
    } else {
        out = "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
    }
    return negative ? "-" + out : out;
}

class EnvScope {
public:
    EnvScope() = default;
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
    ~EnvScope() {
        for (const auto& [name, saved] : saved_) {
            if (saved.has_value()) ::setenv(name.c_str(), saved->c_str(), 1);
            else ::unsetenv(name.c_str());
        }
    }
    void set(const std::string& name, const std::string& value) {
        remember(name);
        ::setenv(name.c_str(), value.c_str(), 1);
    }
    void unset(const std::string& name) {
        remember(name);
        ::unsetenv(name.c_str());
    }

private:
    void remember(const std::string& name) {
        if (saved_.count(name) != 0) return;
        const char* old = std::getenv(name.c_str());
        saved_.emplace(name, old != nullptr ? std::optional<std::string>(old) : std::nullopt);
    }
    std::map<std::string, std::optional<std::string>> saved_;
};

// where the stand-ins are: fakebin/ beside this executable (tools/CMakeLists.txt builds them there), found at run time and not compiled in
fs::path fakebin() { return fs::canonical("/proc/self/exe").parent_path() / "fakebin"; }

const std::vector<double> kBaseAltitudes = {0.0,  5.0,  10.0, 15.0,  20.0,  25.0,  32.5,   40.0,   45.0,   55.0,   62.5,   70.0,   72.5,   80.0,
                                            90.0, 100.0, 110.0, 120.0, 150.0, 200.0, 300.0, 400.0, 550.0, 700.0, 1000.0, 1500.0, 2000.0};
// a close pair, 10 m either side, of each of the seven species-correction cutoffs (160, 200, 240, 250, 300, 320, 450 km)
const std::vector<double> kStraddle = {159.99, 160.01, 199.99, 200.01, 239.99, 240.01, 249.99, 250.01, 299.99, 300.01, 319.99, 320.01, 449.99, 450.01};

struct Cond {
    int iday;
    double sec, lat, lon, f107a, f107, ap;
};
const std::vector<Cond> kConditions = {{172, 29000.0, 60.0, -70.0, 150.0, 150.0, 4.0},
                                       {81, 29000.0, 0.0, 0.0, 70.0, 70.0, 0.0},
                                       {355, 75000.0, -80.0, 170.0, 250.0, 300.0, 200.0},
                                       {200, 43200.0, 45.0, 90.0, 100.0, 90.0, 15.0}};

std::vector<double> all_altitudes() {
    std::vector<double> v = kBaseAltitudes;
    v.insert(v.end(), kStraddle.begin(), kStraddle.end());
    return v;
}

// ---- the stand-in reference ---------------------------------------------------------------------------------------------------------------------------------------------------------------------

// One record of the numbers the stand-in prints, in single and in double precision.  All values are chosen so that their ratios are exact in binary (powers of two and small integers times 1 + m/64, 1 + m/1024),
// which makes every relative difference an exact multiple of 1/1024 and every tie exact.
struct Rec {
    char set;
    int index;
    std::array<double, 11> d{}, s{};
    double drho = 0.0, srho = 0.0;
};

std::vector<Rec> make_records() {
    std::vector<Rec> recs;
    for (int k = 0; k < 181; ++k) {
        Rec r;
        r.set = k < 17 ? 'P' : 'S';
        r.index = k < 17 ? k + 1 : k - 16;
        const double grow = 1.0 + static_cast<double>(k % 7) / 64.0;
        r.d = {1000.0 * grow, 2000.0 * grow, 3000.0 * grow, 4000.0 * grow, (k % 10 == 0) ? 1.0 / 1024.0 : 5000.0 * grow, 0x1p-20, 6000.0 * grow, 0.0, (k % 3 == 0) ? 0.0 : 8000.0 * grow, 1000.0 + k, 900.0 + k};
        for (std::size_t j = 0; j < 11; ++j) {
            const int m = (k % 10 == 0 && j == 4) ? 8 : static_cast<int>((static_cast<std::size_t>(k) + j) % 4);
            r.s.at(j) = r.d.at(j) * (1.0 + static_cast<double>(m) / 1024.0);
        }
        if (k % 13 == 0) r.s.at(1) = 0.0;     // O: single returns exactly zero where double does not (class C)
        r.s.at(7) = 7.0;                      // N: zero in double, not in single (class Z takes it)
        if (k % 3 == 0) r.s.at(8) = 1.0;      // anomalous O: likewise
        r.drho = 0x1p-21 * (1.0 + static_cast<double>(k % 9) / 64.0);
        r.srho = r.drho * (1.0 + static_cast<double>(k % 3) / 1024.0);
        recs.push_back(r);
    }
    return recs;
}

// the text the Fortran prints: "G7 <set><I5>" and eleven  1X,1PE25.17  numbers; "G7D <set><I5>" and one
std::string reference_text(const std::vector<Rec>& recs, bool single) {
    std::string t;
    for (const Rec& r : recs) {
        char head[64];
        std::snprintf(head, sizeof head, "G7 %c%5d", r.set, r.index);
        t += head;
        const std::array<double, 11>& v = single ? r.s : r.d;
        for (const double x : v) t += " " + fmt("%25.17E", x);
        t += "\n";
        std::snprintf(head, sizeof head, "G7D %c%5d", r.set, r.index);
        t += head;
        t += " " + fmt("%25.17E", single ? r.srho : r.drho) + "\n";
    }
    return t;
}

constexpr double kAmu = 1.66e-24;
const std::map<std::size_t, double> kMass = {{0, 4.0}, {1, 16.0}, {2, 28.0}, {3, 32.0}, {4, 40.0}, {6, 1.0}, {7, 14.0}, {8, 16.0}};

// SECOND METHOD for the classification: loops over the numbers, written as the statements of the Python's classify() read, with std::tuple for its (r, key, quantity) ordering
struct Brute {
    using Entry = std::tuple<double, std::string, std::string, long long, std::size_t>;   // r, kind, set, index, quantity
    std::vector<Entry> material, immaterial;
    std::size_t underflowed = 0, zeros = 0;
    std::vector<std::tuple<std::string, std::string, long long, std::size_t, double>> underflows;   // kind, set, index, quantity, double
};

Brute brute_classify(const std::vector<Rec>& recs, double material) {
    Brute b;
    for (const Rec& rec : recs) {
        for (const bool g7 : {true, false}) {
            const std::size_t n = g7 ? 11 : 1;
            for (std::size_t j = 0; j < n; ++j) {
                const double xd = g7 ? rec.d.at(j) : rec.drho;
                const double xs = g7 ? rec.s.at(j) : rec.srho;
                const double rho = g7 ? rec.d.at(5) : rec.drho;
                if (xd == 0.0) {
                    ++b.zeros;
                    continue;
                }
                if (xs == 0.0) {
                    ++b.underflowed;
                    b.underflows.emplace_back(g7 ? "g7" : "g7d", std::string(1, rec.set), rec.index, j, xd);
                    continue;
                }
                const double r = std::fabs(xs - xd) / std::fabs(xd);
                const double frac = (g7 && kMass.count(j) != 0) ? kMass.at(j) * xd * kAmu / rho : 1.0;
                (frac >= material ? b.material : b.immaterial).emplace_back(r, g7 ? "g7" : "g7d", std::string(1, rec.set), rec.index, j);
            }
        }
    }
    std::sort(b.material.rbegin(), b.material.rend());   // ascending order read backwards: the reverse sort of the tuples
    std::sort(b.immaterial.rbegin(), b.immaterial.rend());
    return b;
}

// the place of a sweep record, from the tables above (the tool's describe() is tested by hand apart)
std::string place_of(const std::string& set, long long index) {
    if (set == "P") return "published case " + std::to_string(index);
    const std::vector<double> alts = all_altitudes();
    const std::size_t n = static_cast<std::size_t>(index - 1) % alts.size();
    const Cond& c = kConditions.at(static_cast<std::size_t>(index - 1) / alts.size());
    return "sweep alt " + fmt("%g", alts.at(n)) + " km, condition (" + std::to_string(c.iday) + "," + fmt("%g", c.sec) + "," + fmt("%g", c.lat) + "," + fmt("%g", c.lon) + "," + fmt("%g", c.f107a) + "," + fmt("%g", c.f107) +
           "," + fmt("%g", c.ap) + ")";
}

const std::vector<std::string> kSpecies = {"He", "O", "N2", "O2", "Ar", "rho", "H", "N", "anomalous O", "Tinf", "T(alt)"};

// ---- a tree and the stand-in compiler on PATH ---------------------------------------------------------------------------------------------------------------------------------------------------------

// a synthetic cached Fortran of `lines` lines ("      C line N")
std::string synthetic_fortran(std::size_t lines) {
    std::string t;
    for (std::size_t i = 1; i <= lines; ++i) t += "      C line " + std::to_string(i) + "\n";
    return t;
}

struct Tree {
    TempDir td{"odl-mr"};
    fs::path root = td.path();
    fs::path bin = root / "bin";
    fs::path scratch = root / "scratch";
    std::string fortran;
    std::string sha;
    std::vector<Rec> recs = make_records();
    EnvScope env;

    explicit Tree(std::size_t fortran_lines = 3000) : fortran(synthetic_fortran(fortran_lines)), sha(sha256_hex(as_bytes(fortran))) {
        fs::create_directories(root / "manifest");
        fs::create_directories(root / "cache" / "nrlmsise00-fortran");
        fs::create_directories(root / "modules" / "atmosphere" / "src");
        fs::create_directories(bin);
        fs::create_directories(scratch / "capture");
        write_text(root / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR", fortran);
        write_text(root / "manifest" / "manifest.json",
                   "{\"cache\": \"cache\", \"entries\": [{\"id\": \"other\", \"filename\": \"x\", \"sha256\": \"00\"}, {\"id\": \"nrlmsise00-fortran\", \"filename\": \"NRLMSISE-00.FOR\", \"sha256\": \"" + sha + "\"}]}");
        install("gfortran");
        write_text(scratch / "single.txt", reference_text(recs, true));
        write_text(scratch / "double.txt", reference_text(recs, false));
        env.set("ODL_FAKE_REF_OUT_REF_S", (scratch / "single.txt").string());
        env.set("ODL_FAKE_REF_OUT_REF_D", (scratch / "double.txt").string());
        env.set("ODL_FAKE_REF_STDIN_REF_S", (scratch / "stdin_s.txt").string());
        env.set("ODL_FAKE_REF_STDIN_REF_D", (scratch / "stdin_d.txt").string());
        env.set("ODL_FAKE_GFORTRAN_LOG", (scratch / "calls.log").string());
        env.set("ODL_FAKE_GFORTRAN_CAPTURE", (scratch / "capture").string());
        for (const char* steering : {"ODL_FAKE_GFORTRAN_VERSION", "ODL_FAKE_GFORTRAN_EXIT", "ODL_FAKE_GFORTRAN_STDERR", "ODL_FAKE_GFORTRAN_EXE_MODE", "ODL_FAKE_REF_EXIT_REF_S", "ODL_FAKE_REF_EXIT_REF_D",
                                      "ODL_FAKE_REF_STDERR_REF_S", "ODL_FAKE_REF_STDERR_REF_D"})
            env.unset(steering);
    }
    // a copy of the stand-in compiler under the given name in bin/
    void install(const std::string& name) const {
        REQUIRE(fs::exists(fakebin() / "gfortran"));
        fs::copy_file(fakebin() / "gfortran", bin / name, fs::copy_options::overwrite_existing);
        fs::copy_file(fakebin() / "odl_fake_reference", bin / "odl_fake_reference", fs::copy_options::overwrite_existing);
    }
    [[nodiscard]] ms::Settings settings() const {
        ms::Settings s;
        s.root = root;
        s.path = bin.string();
        return s;
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const { return run_with(settings(), extra); }
    [[nodiscard]] fs::path header() const { return root / "modules" / "atmosphere" / "src" / "msis_reference_values.hpp"; }
    [[nodiscard]] fs::path work() const { return root / "tools" / ".msisref"; }
    [[nodiscard]] std::vector<std::vector<std::string>> calls() const {
        std::vector<std::vector<std::string>> out;
        const fs::path log = scratch / "calls.log";
        if (!fs::exists(log)) return out;
        std::istringstream in(read_text(log));
        std::string line;
        while (std::getline(in, line)) {
            if (line == "--- call") out.emplace_back();
            else if (line != "--- end" && !out.empty()) out.back().push_back(line);
        }
        return out;
    }
};

const char kUsage[] = "usage: msis_reference [-h] [--root ROOT] [--out OUT] [--check] [--work WORK] [--sweep-input SWEEP_INPUT]\n";

// What `-h` prints after the usage line: the port's own text (the Python's argparse help was wrapped to the terminal's width and cannot be reproduced byte for byte), RECORDED here whole so that every word of it is
// pinned, not only the lines that the fragments in the command-line case look at.
const char kHelp[] =
    "\n"
    "Freeze the NRLMSISE-00 reference implementation's output (SPEC-atmosphere ATMO-R-028): NRL's published Fortran, unedited, built twice with the HOST PROGRAM gfortran (as published, and with every REAL\n"
    "widened), run on the 17 published cases and a sweep, classified and written as modules/atmosphere/src/msis_reference_values.hpp.  Regeneration only: nothing in the build, the tests or CI runs it.\n"
    "With no Fortran compiler on PATH it says so and does nothing.\n"
    "\n"
    "options:\n"
    "  -h, --help                 show this help and exit\n"
    "  --root ROOT                the tree to read the manifest and the cache of, and to write into (default: the tree this tool was built from)\n"
    "  --out OUT                  the generated header, relative to the tree unless absolute (default: modules/atmosphere/src/msis_reference_values.hpp)\n"
    "  --check                    regenerate and compare, do not write: exit 1 unless the committed header is exactly what the regeneration emits\n"
    "  --work WORK                where the Fortran is built and run (default: tools/.msisref of the tree, git-ignored)\n"
    "  --sweep-input SWEEP_INPUT  also write the standard input of the sweep to this file\n"
    "\n"
    "exit codes: 0 written / the committed header reproduces / no compiler   1 it differs, or a compiler or a reference run failed, or the cached bytes are not the pinned bytes\n"
    "            2 an argument error, or a file that is missing or cannot be read   70 an error the tool did not anticipate\n";

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the sweep

TEST_CASE("the sweep's altitudes and conditions, and the input the Fortran reads", "[msis_reference][behaviour]") {
    CHECK(ms::sweep_alt() == all_altitudes());
    CHECK(ms::sweep_alt().size() == 41);
    CHECK(&ms::sweep_alt() == &ms::sweep_alt());
    // the pairs straddle each cutoff by 10 m, rounded to six decimals as Python's round(c + offset / 1000.0, 6)
    for (std::size_t i = 0; i < kStraddle.size(); i += 2) {
        CHECK(ms::sweep_alt().at(27 + i) < ms::sweep_alt().at(27 + i + 1));
        CHECK(std::fabs(ms::sweep_alt().at(27 + i + 1) - ms::sweep_alt().at(27 + i) - 0.02) < 1e-9);
    }
    const std::vector<ms::Condition>& cond = ms::sweep_cond();
    REQUIRE(cond.size() == kConditions.size());
    for (std::size_t i = 0; i < cond.size(); ++i) {
        INFO("condition " << i);
        CHECK(cond.at(i).iday == kConditions.at(i).iday);
        CHECK(cond.at(i).sec == kConditions.at(i).sec);
        CHECK(cond.at(i).lat == kConditions.at(i).lat);
        CHECK(cond.at(i).lon == kConditions.at(i).lon);
        CHECK(cond.at(i).f107a == kConditions.at(i).f107a);
        CHECK(cond.at(i).f107 == kConditions.at(i).f107);
        CHECK(cond.at(i).ap == kConditions.at(i).ap);
    }
    // the input: the number of cases, then one line per (condition, altitude), conditions outermost, every float as Python's repr writes it, and the solar time sec / 3600 + lon / 15
    const std::vector<std::string> lines = lines_of(ms::sweep_input());
    REQUIRE(lines.size() == 1 + 164);
    CHECK(lines.at(0) == "164");
    std::size_t at = 1;
    for (const Cond& c : kConditions) {
        for (const double alt : all_altitudes()) {
            const double stl = c.sec / 3600.0 + c.lon / 15.0;
            const std::string expected = std::to_string(c.iday) + " " + py_repr(c.sec) + " " + py_repr(alt) + " " + py_repr(c.lat) + " " + py_repr(c.lon) + " " + py_repr(stl) + " " + py_repr(c.f107a) + " " + py_repr(c.f107) +
                                         " " + py_repr(c.ap);
            INFO("line " << at);
            CHECK(lines.at(at) == expected);
            ++at;
        }
    }
    // a few lines by eye: the first of each condition and one straddle value
    CHECK(lines.at(1).rfind("172 29000.0 0.0 60.0 -70.0 ", 0) == 0);
    CHECK(lines.at(1 + 41) == "81 29000.0 0.0 0.0 0.0 8.055555555555555 70.0 70.0 0.0");
    CHECK(lines.at(1 + 41 + 41 + 27) == "355 75000.0 159.99 -80.0 170.0 " + py_repr(75000.0 / 3600.0 + 170.0 / 15.0) + " 250.0 300.0 200.0");
    CHECK(lines.at(1 + 41 + 41 + 41 + 6) == "200 43200.0 32.5 45.0 90.0 18.0 100.0 90.0 15.0");
    CHECK(ms::sweep_input().back() == '\n');
}

TEST_CASE("model_source: the first 2,437 lines of the cached Fortran, each ended by a newline", "[msis_reference][behaviour]") {
    CHECK(ms::kDriverSplit == 2437);
    const std::string long_text = synthetic_fortran(3000);
    const std::string model = ms::model_source(long_text);
    const std::vector<std::string> lines = lines_of(model);
    REQUIRE(lines.size() == 2437);
    CHECK(lines.front() == "      C line 1");
    CHECK(lines.back() == "      C line 2437");
    CHECK(model == synthetic_fortran(2437));
    CHECK(model.back() == '\n');
    // exactly 2437 lines: the whole text (the Python's  "\n".join(text.split("\n")[:2437]) + "\n"  keeps the empty piece after the last newline out of range)
    CHECK(ms::model_source(synthetic_fortran(2437)) == synthetic_fortran(2437));
    // 2438 lines: the last is dropped
    CHECK(ms::model_source(synthetic_fortran(2438)) == synthetic_fortran(2437));
    // fewer than that: the whole text and one more newline (the join of all the pieces, the empty one after the final newline included, plus the newline)
    CHECK(ms::model_source(synthetic_fortran(10)) == synthetic_fortran(10) + "\n");
    CHECK(ms::model_source("a\nb") == "a\nb\n");
    CHECK(ms::model_source("") == "\n");
    CHECK(ms::model_source("\n") == "\n\n");
    // a text of 2437 lines whose last has no newline: the join ends there, and the newline is added
    std::string no_final = synthetic_fortran(2437);
    no_final.pop_back();
    CHECK(ms::model_source(no_final) == synthetic_fortran(2437));
    // only "\n" splits: a carriage return stays (the tool reads the file with universal newlines before this)
    CHECK(ms::model_source("a\rb\nc\n") == "a\rb\nc\n\n");
    // lines longer than the text of a line are no trouble; a text longer than 2437 lines, with blank lines in it, counts them
    std::string blanks(3000, '\n');
    CHECK(ms::model_source(blanks) == std::string(2437, '\n'));
}

TEST_CASE("driver_source: the 17 published cases verbatim and a sweep read from standard input", "[msis_reference][behaviour]") {
    const std::vector<std::string> lines = lines_of(ms::driver_source());
    const std::vector<std::string> expected = {
        "C     GENERATED by tools/msis_reference.cpp -- do not edit.",
        "C     Part 1 replicates NRLMSISE-00.FOR lines 2438-2552's DATA statements",
        "C     EXACTLY; only the output FORMAT differs.  Part 2 reads a sweep.",
        "      DIMENSION D(9),T(2),SW(25),SWD(25),APH(7)",
        "      DIMENSION IDAY(15),UT(15),ALT(15),XLAT(15),XLONG(15),XLST(15),",
        "     & F107A(15),F107(15),AP(15)",
        "      DATA IDAY/172,81,13*172/",
        "      DATA UT/29000.,29000.,75000.,12*29000./",
        "      DATA ALT/400.,400.,1000.,100.,6*400.,0,10.,30.,50.,70./",
        "      DATA XLAT/4*60.,0.,10*60./",
        "      DATA XLONG/5*-70.,0.,9*-70./",
        "      DATA XLST/6*16.,4.,8*16./",
        "      DATA F107A/7*150.,70.,7*150./",
        "      DATA F107/8*150.,180.,6*150./",
        "      DATA AP/9*4.,40.,5*4./",
        "      DATA APH/7*100./,SW/8*1.,-1.,16*1./,SWD/25*1./",
        "      DO I=1,15",
        "         CALL GTD7(IDAY(I),UT(I),ALT(I),XLAT(I),XLONG(I),XLST(I),",
        "     &             F107A(I),F107(I),AP(I),48,D,T)",
        "         CALL WROUT('P',I,D,T)",
        "         CALL GTD7D(IDAY(I),UT(I),ALT(I),XLAT(I),XLONG(I),XLST(I),",
        "     &             F107A(I),F107(I),AP(I),48,D,T)",
        "         CALL WRRHO('P',I,D(6))",
        "      ENDDO",
        "      CALL TSELEC(SW)",
        "      CALL GTD7(IDAY(1),UT(1),ALT(1),XLAT(1),XLONG(1),XLST(1),",
        "     &             F107A(1),F107(1),APH,48,D,T)",
        "      CALL WROUT('P',16,D,T)",
        "      CALL GTD7D(IDAY(1),UT(1),ALT(1),XLAT(1),XLONG(1),XLST(1),",
        "     &             F107A(1),F107(1),APH,48,D,T)",
        "      CALL WRRHO('P',16,D(6))",
        "      CALL GTD7(IDAY(1),UT(1),ALT(4),XLAT(1),XLONG(1),XLST(1),",
        "     &             F107A(1),F107(1),APH,48,D,T)",
        "      CALL WROUT('P',17,D,T)",
        "      CALL GTD7D(IDAY(1),UT(1),ALT(4),XLAT(1),XLONG(1),XLST(1),",
        "     &             F107A(1),F107(1),APH,48,D,T)",
        "      CALL WRRHO('P',17,D(6))",
        "C     ---- part 2: the sweep, one case per line on stdin ----",
        "      CALL TSELEC(SWD)",
        "      READ(5,*) NS",
        "      DO K=1,NS",
        "         READ(5,*) JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD",
        "         CALL GTD7(JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD,48,D,T)",
        "         CALL WROUT('S',K,D,T)",
        "         CALL GTD7D(JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD,48,D,T)",
        "         CALL WRRHO('S',K,D(6))",
        "      ENDDO",
        "      END",
        "      FUNCTION SW2()",
        "      DIMENSION SW2(25)",
        "      DO J=1,25",
        "         SW2(J)=1.",
        "      ENDDO",
        "      END",
        "      SUBROUTINE WROUT(TAG,I,D,T)",
        "      CHARACTER*1 TAG",
        "      DIMENSION D(9),T(2)",
        "      WRITE(6,100) TAG,I,(D(J),J=1,9),T(1),T(2)",
        "  100 FORMAT('G7 ',A1,I5,11(1X,1PE25.17))",
        "      END",
        "      SUBROUTINE WRRHO(TAG,I,R)",
        "      CHARACTER*1 TAG",
        "      WRITE(6,101) TAG,I,R",
        "  101 FORMAT('G7D ',A1,I5,1X,1PE25.17)",
        "      END",
    };
    REQUIRE(lines.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("line " << (i + 1));
        CHECK(lines.at(i) == expected.at(i));
    }
    CHECK(ms::driver_source().back() == '\n');
    CHECK(lines.size() == 65);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the reference's output

TEST_CASE("parse_output: the lines G7 and G7D, keys and numbers, by hand", "[msis_reference][behaviour]") {
    const std::string text =
        "G7 P    1   1.50000000000000000E+03  -2.50000000000000000E-01   0.00000000000000000E+00   1.0E0  2.0E0 3.0E0 4.0E0 5.0E0 6.0E0 7.0E0 8.0E0\n"
        "G7D P    1   3.14159265358979312E-15\n"
        "G7 S  164   1.0E+00 2.0E+00 3.0E+00\n"
        "G7D S  164   9.0E-30   99.0   100.0\n";
    const ms::Results r = ms::parse_output(text);
    REQUIRE(r.items.size() == 4);
    CHECK(r.items.at(0).first == ms::Key{"g7", "P", 1});
    CHECK(r.items.at(0).second == std::vector<double>{1500.0, -0.25, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0});
    CHECK(r.items.at(1).first == ms::Key{"g7d", "P", 1});
    CHECK(r.items.at(1).second == std::vector<double>{3.14159265358979312e-15});
    CHECK(r.items.at(2).first == ms::Key{"g7", "S", 164});
    CHECK(r.items.at(2).second == std::vector<double>{1.0, 2.0, 3.0});   // any number of values after the index
    CHECK(r.items.at(3).first == ms::Key{"g7d", "S", 164});
    CHECK(r.items.at(3).second == std::vector<double>{9.0e-30});          // G7D takes the first number only
    // find: by key, null when absent
    REQUIRE(r.find(ms::Key{"g7", "P", 1}) != nullptr);
    CHECK(r.find(ms::Key{"g7", "P", 1})->size() == 11);
    CHECK(r.find(ms::Key{"g7", "P", 2}) == nullptr);
    CHECK(r.find(ms::Key{"g7d", "P", 1})->size() == 1);
    CHECK(r.find(ms::Key{"g7", "S", 1}) == nullptr);
    // anything else is ignored: blank lines, other words, a line of the wrong case, text before the first G7 line
    const ms::Results other = ms::parse_output("\n   \nhello 1 2 3\ng7 P 1 1.0\nSTOP\n   G7D   P   2    5.5   \n\nG7 P 3\n");
    REQUIRE(other.items.size() == 2);
    CHECK(other.items.at(0).first == ms::Key{"g7d", "P", 2});
    CHECK(other.items.at(0).second == std::vector<double>{5.5});
    CHECK(other.items.at(1).first == ms::Key{"g7", "P", 3});
    CHECK(other.items.at(1).second.empty());   // a G7 line with no number after the index
    // CRLF line ends and a line without a final newline
    const ms::Results crlf = ms::parse_output("G7 P 1 1.0 2.0\r\nG7D P 1 3.0");
    REQUIRE(crlf.items.size() == 2);
    CHECK(crlf.items.at(1).second == std::vector<double>{3.0});
    // a key that comes twice keeps its first place and takes the new numbers (a Python dict)
    const ms::Results twice = ms::parse_output("G7 P 1 1.0\nG7 P 2 2.0\nG7 P 1 3.0\n");
    REQUIRE(twice.items.size() == 2);
    CHECK(twice.items.at(0).first == ms::Key{"g7", "P", 1});
    CHECK(twice.items.at(0).second == std::vector<double>{3.0});
    // an index with a plus sign is an integer; the set is any word
    const ms::Results plus = ms::parse_output("G7 SW +7 1.0\n");
    REQUIRE(plus.items.size() == 1);
    CHECK(plus.items.at(0).first == ms::Key{"g7", "SW", 7});
    // Fortran prints infinities and not-a-numbers in words
    const ms::Results words = ms::parse_output("G7 P 1 Infinity -Infinity NaN\n");
    REQUIRE(words.items.size() == 1);
    CHECK(std::isinf(words.items.at(0).second.at(0)));
    CHECK(words.items.at(0).second.at(1) < 0.0);
    CHECK(std::isnan(words.items.at(0).second.at(2)));
    CHECK(ms::parse_output("").items.empty());
    // refusals: too short, an index that is no integer, a number that does not parse
    const auto message_of = [](const std::string& t) {
        try {
            (void)ms::parse_output(t);
        } catch (const ms::BadInput& e) {
            return std::string(e.what());
        }
        return std::string("(nothing was thrown)");
    };
    CHECK(message_of("G7\n") == "a line of the reference's output is too short: G7");
    CHECK(message_of("G7 P\n") == "a line of the reference's output is too short: G7 P");
    CHECK(message_of("G7D P 5\n") == "a line of the reference's output is too short: G7D P 5");
    CHECK(message_of("G7D\n") == "a line of the reference's output is too short: G7D");
    CHECK(message_of("G7 P x 1.0\n") == "the index in a line of the reference's output is not an integer: G7 P x 1.0");
    CHECK(message_of("G7 P 1.5 1.0\n") == "the index in a line of the reference's output is not an integer: G7 P 1.5 1.0");
    CHECK(message_of("G7 P 5x 1.0\n") == "the index in a line of the reference's output is not an integer: G7 P 5x 1.0");
    CHECK(message_of("G7 P 1 1.0 abc 3.0\n") == "a number in a line of the reference's output does not parse: abc");
    CHECK(message_of("G7D P 1 1.0.0\n") == "a number in a line of the reference's output does not parse: 1.0.0");
    CHECK(message_of("G7 P 1 1.0 2.0E\n") == "a number in a line of the reference's output does not parse: 2.0E");
    CHECK(message_of("G7D P 1 ****\n") == "a number in a line of the reference's output does not parse: ****");   // Fortran's overflow marker
    CHECK(message_of("G7D P 1 1.0 ****\n") == "(nothing was thrown)");                                              // the extra field of a G7D line is never read
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the classification

namespace {

ms::Key g7(const char* set, long long index) { return ms::Key{"g7", set, index}; }
ms::Key g7d(const char* set, long long index) { return ms::Key{"g7d", set, index}; }

}  // namespace

TEST_CASE("classify: the four classes, by hand on a record whose every ratio is exact", "[msis_reference][behaviour]") {
    const double big = 0x1p40;       // 1.1e12
    const double tiny = 0x1p-10;     // 9.8e-4
    ms::Results dbl;
    ms::Results sgl;
    //                     He       O      N2       O2        Ar     rho   H     N    anomO   Tinf     T
    dbl.set(g7("P", 1), {big, big, big, big, tiny, 1.0, big, 0.0, 0.0, 1024.0, 512.0});
    sgl.set(g7("P", 1), {1.5 * big, big, 0.75 * big, 0.0, 1.5 * tiny, 1.0, big, 7.0, 5.0, 1152.0, 512.0});
    dbl.set(g7d("P", 1), {2.0});
    sgl.set(g7d("P", 1), {3.0});
    const ms::Classes c = ms::classify(sgl, dbl);
    // zeros: N and anomalous O are zero in double (whatever single says); underflowed: O2 (single 0, double not)
    CHECK(c.zeros == 2);
    REQUIRE(c.underflowed.size() == 1);
    CHECK(c.underflowed.at(0).key == g7("P", 1));
    CHECK(c.underflowed.at(0).quantity == 3);
    CHECK(c.underflowed.at(0).value == big);
    // immaterial: Ar (mass 40 * 2^-10 * 1.66e-24 / rho 1 = 6.5e-26 < 1e-15), r = 0.5
    REQUIRE(c.immaterial.size() == 1);
    CHECK(c.immaterial.at(0).relative == 0.5);
    CHECK(c.immaterial.at(0).key == g7("P", 1));
    CHECK(c.immaterial.at(0).quantity == 4);
    // material: He (r 0.5), N2 (0.25), Tinf (0.125), the total density of GTD7D (0.5), and the exact ones (O, rho, H, T: r 0); sorted by r, then key, then quantity, all descending
    //   (0.5, g7d P1, 0)  (0.5, g7 P1, 0)  (0.25, g7 P1, 2)  (0.125, g7 P1, 9)  (0, g7 P1, 10)  (0, g7 P1, 6)  (0, g7 P1, 5)  (0, g7 P1, 1)
    REQUIRE(c.material.size() == 8);
    const std::vector<std::tuple<double, ms::Key, std::size_t>> expected = {{0.5, g7d("P", 1), 0}, {0.5, g7("P", 1), 0}, {0.25, g7("P", 1), 2}, {0.125, g7("P", 1), 9},
                                                                            {0.0, g7("P", 1), 10},  {0.0, g7("P", 1), 6},  {0.0, g7("P", 1), 5}, {0.0, g7("P", 1), 1}};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("material " << i);
        CHECK(c.material.at(i).relative == std::get<0>(expected.at(i)));
        CHECK(c.material.at(i).key == std::get<1>(expected.at(i)));
        CHECK(c.material.at(i).quantity == std::get<2>(expected.at(i)));
    }
    // total = 8 + 1 + 1 + 2 = 12 = 1 record x 12 quantities
    CHECK(c.material.size() + c.immaterial.size() + c.underflowed.size() + c.zeros == 12);
    // the materiality threshold is a parameter: 1e-25 makes Ar (6.5e-26 -- not yet) immaterial still, 1e-27 makes it material
    CHECK(ms::classify(sgl, dbl, 1e-25).immaterial.size() == 1);
    const ms::Classes low = ms::classify(sgl, dbl, 1e-27);
    CHECK(low.immaterial.empty());
    CHECK(low.material.size() == 9);
    // ... and a high one moves the lighter species to the immaterial: at 1e-11 He (4 * 2^40 * 1.66e-24 = 7.3e-12) and H (1.8e-12) and Ar join Ar's side, O (2.9e-11) and N2 (5.1e-11) stay material, and the non-species
    // (rho, temperatures, GTD7D) are always material
    const ms::Classes high = ms::classify(sgl, dbl, 1e-11);
    CHECK(high.material.size() == 6);        // O, N2, rho, Tinf, T and the total density of GTD7D
    CHECK(high.immaterial.size() == 3);      // He, Ar, H
    CHECK(ms::kMaterial == 1e-15);
    // a quantity that is no species (the total density, the two temperatures, every GTD7D value) is the whole: its fraction is exactly 1, so it is material at the threshold 1 -- a fraction equal to the
    // threshold is material -- and immaterial at any threshold above it; the species, fractions of 1e-11 and less, are immaterial at both
    const ms::Classes whole = ms::classify(sgl, dbl, 1.0);
    CHECK(whole.material.size() == 4);       // rho, Tinf, T and the total density of GTD7D
    CHECK(whole.immaterial.size() == 5);     // He, O, N2, H, Ar
    CHECK(ms::classify(sgl, dbl, 1.0000001).material.empty());
    CHECK(ms::classify(sgl, dbl, 1.0000001).immaterial.size() == 9);
}

TEST_CASE("classify: the mass of each species, to a tenth of a percent -- a fraction just above the threshold is material, one just below is not", "[msis_reference][behaviour]") {
    // the fraction of the total density that species j makes up is  mass * n * 1.66e-24 / rho;  with rho = 1, the number density n = (1 +- 1/1000) * threshold / (mass * 1.66e-24) puts it a tenth of a percent
    // above or below the threshold.  The masses are the nuclear ones in atomic mass units: He 4, O 16, N2 28, O2 32, Ar 40, H 1, N 14, anomalous O 16.
    const double threshold = 1e-15;
    const double amu = 1.66e-24;
    struct Species {
        std::size_t j;
        double mass;
        const char* name;
    };
    for (const Species sp : {Species{0, 4.0, "He"}, Species{1, 16.0, "O"}, Species{2, 28.0, "N2"}, Species{3, 32.0, "O2"}, Species{4, 40.0, "Ar"}, Species{6, 1.0, "H"}, Species{7, 14.0, "N"}, Species{8, 16.0, "anomalous O"}}) {
        for (const bool above : {true, false}) {
            const double n = (above ? 1.001 : 0.999) * threshold / (sp.mass * amu);
            ms::Results dbl;
            ms::Results sgl;
            std::vector<double> d(11, 0.0);
            std::vector<double> s(11, 0.0);
            d.at(5) = 1.0;     // the total density
            s.at(5) = 1.0;
            d.at(sp.j) = n;
            s.at(sp.j) = 1.5 * n;
            dbl.set(g7("P", 1), d);
            sgl.set(g7("P", 1), s);
            const ms::Classes c = ms::classify(sgl, dbl, threshold);
            const auto has = [&](const std::vector<ms::Comparison>& v) {
                return std::any_of(v.begin(), v.end(), [&](const ms::Comparison& x) { return x.quantity == sp.j; });
            };
            INFO(sp.name << (above ? " a tenth of a percent above the threshold" : " a tenth of a percent below the threshold"));
            CHECK(has(above ? c.material : c.immaterial));
            CHECK_FALSE(has(above ? c.immaterial : c.material));
            // and nothing else but the density itself (exact, material) was compared
            CHECK(c.material.size() + c.immaterial.size() == 2);
        }
    }
}

TEST_CASE("classify: the ordering of ties, several records, shorter lists, and the refusals", "[msis_reference][behaviour]") {
    ms::Results dbl;
    ms::Results sgl;
    const double big = 0x1p40;
    // three records whose He differ by 0.5 each: ties in r are ordered by key (kind, then set, then index), descending, then by quantity
    for (const auto& [set, index] : std::vector<std::pair<const char*, long long>>{{"P", 1}, {"P", 2}, {"S", 1}}) {
        dbl.set(g7(set, index), {big, big, big, big, big, 1.0, big, big, big, big, big});
        sgl.set(g7(set, index), {1.5 * big, big, big, big, big, 1.0, big, big, big, big, 1.5 * big});
    }
    const ms::Classes c = ms::classify(sgl, dbl);
    REQUIRE(c.material.size() == 33);
    // r = 0.5 for He (0) and T (10) of each record: six of them, first the S record (set 'S' > 'P'), then P2, then P1, and within a record quantity 10 before 0
    CHECK(c.material.at(0).key == g7("S", 1));
    CHECK(c.material.at(0).quantity == 10);
    CHECK(c.material.at(1).key == g7("S", 1));
    CHECK(c.material.at(1).quantity == 0);
    CHECK(c.material.at(2).key == g7("P", 2));
    CHECK(c.material.at(2).quantity == 10);
    CHECK(c.material.at(3).key == g7("P", 2));
    CHECK(c.material.at(4).key == g7("P", 1));
    CHECK(c.material.at(5).key == g7("P", 1));
    CHECK(c.material.at(5).quantity == 0);
    for (std::size_t i = 6; i < c.material.size(); ++i) CHECK(c.material.at(i).relative == 0.0);   // the rest are exact
    // the index orders as a number, not as text: index 10 sorts above index 9
    ms::Results d2;
    ms::Results s2;
    for (const long long index : {9LL, 10LL}) {
        d2.set(g7("S", index), {big, big, big, big, big, 1.0, big, big, big, big, big});
        s2.set(g7("S", index), {1.5 * big, big, big, big, big, 1.0, big, big, big, big, big});
    }
    const ms::Classes c2 = ms::classify(s2, d2);
    CHECK(c2.material.at(0).key == g7("S", 10));
    CHECK(c2.material.at(1).key == g7("S", 9));
    // a kind 'g7' record sorts below a 'g7d' one of the same r and set (the strings compare as "g7" < "g7d")
    ms::Results d3;
    ms::Results s3;
    d3.set(g7("S", 1), {big, big, big, big, big, 1.0, big, big, big, big, big});
    s3.set(g7("S", 1), {1.5 * big, big, big, big, big, 1.0, big, big, big, big, big});
    d3.set(g7d("S", 1), {4.0});
    s3.set(g7d("S", 1), {6.0});
    const ms::Classes c3 = ms::classify(s3, d3);
    CHECK(c3.material.at(0).key == g7d("S", 1));
    CHECK(c3.material.at(1).key == g7("S", 1));
    // a shorter single-precision record is compared as far as it goes (the Python's zip): two numbers against eleven
    ms::Results d4;
    ms::Results s4;
    d4.set(g7("P", 1), {big, big, big, big, big, 1.0, big, big, big, big, big});
    s4.set(g7("P", 1), {1.5 * big, big});
    const ms::Classes c4 = ms::classify(s4, d4);
    CHECK(c4.material.size() + c4.immaterial.size() + c4.underflowed.size() + c4.zeros == 2);
    // an empty classification is possible (nothing to compare)
    const ms::Classes empty = ms::classify(ms::Results{}, ms::Results{});
    CHECK(empty.material.empty());
    CHECK(empty.zeros == 0);
    // refusals: a record in double with no counterpart in single; a record too short to hold the total density
    const auto message_of = [](const ms::Results& s, const ms::Results& d) {
        try {
            (void)ms::classify(s, d);
        } catch (const ms::BadInput& e) {
            return std::string(e.what());
        }
        return std::string("(nothing was thrown)");
    };
    ms::Results lone;
    lone.set(g7("S", 12), {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
    CHECK(message_of(ms::Results{}, lone) == "the single-precision run has no record g7 S12");
    ms::Results lone_d;
    lone_d.set(g7d("P", 3), {1.0});
    CHECK(message_of(ms::Results{}, lone_d) == "the single-precision run has no record g7d P3");
    ms::Results short_d;
    short_d.set(g7("P", 1), {1.0, 2.0, 3.0, 4.0, 5.0});   // five numbers: index 5 (the density) is missing
    ms::Results short_s;
    short_s.set(g7("P", 1), {1.0, 2.0, 3.0, 4.0, 5.0});
    CHECK(message_of(short_s, short_d) == "the record g7 P1 is too short");
    ms::Results empty_d;
    empty_d.set(g7d("P", 1), {});
    ms::Results empty_s;
    empty_s.set(g7d("P", 1), {});
    CHECK(message_of(empty_s, empty_d) == "the record g7d P1 is too short");
    // six numbers are enough for a g7 record
    ms::Results six_d;
    six_d.set(g7("P", 1), {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
    ms::Results six_s;
    six_s.set(g7("P", 1), {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
    CHECK(message_of(six_s, six_d) == "(nothing was thrown)");
}

TEST_CASE("Key and Results: ordering, equality, and a repeated key", "[msis_reference][behaviour]") {
    CHECK(ms::Key{"g7", "P", 1} == ms::Key{"g7", "P", 1});
    CHECK(ms::Key{"g7", "P", 1} != ms::Key{"g7", "P", 2});
    CHECK(ms::Key{"g7", "P", 1} != ms::Key{"g7", "S", 1});
    CHECK(ms::Key{"g7", "P", 1} != ms::Key{"g7d", "P", 1});
    CHECK(ms::Key{"g7", "S", 1} < ms::Key{"g7d", "P", 1});   // the kind first
    CHECK(ms::Key{"g7", "P", 99} < ms::Key{"g7", "S", 1});   // then the set
    CHECK(ms::Key{"g7", "P", 2} < ms::Key{"g7", "P", 10});   // then the index as a number
    ms::Results r;
    r.set(g7("P", 1), {1.0});
    r.set(g7("P", 2), {2.0});
    r.set(g7("P", 1), {3.0, 4.0});
    REQUIRE(r.items.size() == 2);
    CHECK(r.items.at(0).second == std::vector<double>{3.0, 4.0});
    CHECK(r.find(g7("P", 2))->front() == 2.0);
    const ms::Key defaulted;
    CHECK(defaulted.index == 0);
    CHECK(defaulted.kind.empty());
}

TEST_CASE("describe: a comparison's place, from the sweep's own loop order", "[msis_reference][behaviour]") {
    CHECK(ms::describe(g7("P", 4)) == "published case 4");
    CHECK(ms::describe(g7("P", 17)) == "published case 17");
    CHECK(ms::describe(g7d("P", 1)) == "published case 1");
    // the sweep's index runs over the 41 altitudes within the first condition, then the 41 of the second, and so on (1-based); the numbers are written with %g
    CHECK(ms::describe(g7("S", 1)) == "sweep alt 0 km, condition (172,29000,60,-70,150,150,4)");
    CHECK(ms::describe(g7("S", 2)) == "sweep alt 5 km, condition (172,29000,60,-70,150,150,4)");
    CHECK(ms::describe(g7("S", 7)) == "sweep alt 32.5 km, condition (172,29000,60,-70,150,150,4)");
    CHECK(ms::describe(g7("S", 41)) == "sweep alt 450.01 km, condition (172,29000,60,-70,150,150,4)");
    CHECK(ms::describe(g7("S", 42)) == "sweep alt 0 km, condition (81,29000,0,0,70,70,0)");
    CHECK(ms::describe(g7("S", 83)) == "sweep alt 0 km, condition (355,75000,-80,170,250,300,200)");
    CHECK(ms::describe(g7("S", 83 + 27)) == "sweep alt 159.99 km, condition (355,75000,-80,170,250,300,200)");
    CHECK(ms::describe(g7("S", 124)) == "sweep alt 0 km, condition (200,43200,45,90,100,90,15)");
    CHECK(ms::describe(g7("S", 164)) == "sweep alt 450.01 km, condition (200,43200,45,90,100,90,15)");
    CHECK(ms::describe(g7("S", 156)) == "sweep alt 240.01 km, condition (200,43200,45,90,100,90,15)");   // the worst comparison of the committed header
    // an index beyond the 164 cases has no condition
    CHECK_THROWS_AS(ms::describe(g7("S", 165)), std::out_of_range);
    // every sweep index agrees with the test's own table
    for (long long i = 1; i <= 164; ++i) CHECK(ms::describe(g7("S", i)) == place_of("S", i));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- which

TEST_CASE("which: shutil.which on a PATH-like string", "[msis_reference][behaviour]") {
    const TempDir td("odl-which");
    const fs::path a = td.path() / "a";
    const fs::path b = td.path() / "b";
    const fs::path c = td.path() / "c";
    fs::create_directories(a);
    fs::create_directories(b);
    fs::create_directories(c);
    const auto make = [](const fs::path& file, mode_t mode) {
        write_text(file, "x");
        ::chmod(file.c_str(), mode);
    };
    make(a / "tool", 0644);            // a file that is not executable
    make(b / "tool", 0755);
    make(c / "tool", 0755);
    fs::create_directories(a / "dirx");   // a directory named like a program
    fs::create_directories(b / "dirx");
    make(c / "dirx-not", 0755);
    const std::string path = a.string() + ":" + b.string() + ":" + c.string();
    CHECK(ms::which("tool", path) == b.string() + "/tool");                  // the first directory with an executable file of the name
    CHECK(ms::which("tool", c.string() + ":" + b.string()) == c.string() + "/tool");
    CHECK(ms::which("tool", a.string()).empty());                              // present but not executable
    CHECK(ms::which("dirx", path).empty());                                    // a directory is not a program, whatever its permissions
    CHECK(ms::which("missing", path).empty());
    CHECK(ms::which("tool", "").empty());
    CHECK(ms::which("tool", (td.path() / "nowhere").string() + ":" + c.string()) == c.string() + "/tool");   // a directory that does not exist is passed over
    CHECK(ms::which("tool", b.string() + ":" + b.string()) == b.string() + "/tool");                         // duplicates are looked at once
    // a name with a directory part is not looked up
    CHECK(ms::which((b / "tool").string(), path).empty());
    CHECK(ms::which("sub/tool", path).empty());
    // an empty entry is the current directory
    {
        const fs::path saved = fs::current_path();
        fs::current_path(c);
        CHECK(ms::which("tool", ":" + a.string()) == "tool");
        CHECK(ms::which("tool", a.string() + "::" + b.string()) == "tool");
        fs::current_path(saved);
    }
    // a trailing colon too
    {
        const fs::path saved = fs::current_path();
        fs::current_path(b);
        CHECK(ms::which("tool", a.string() + ":") == "tool");
        fs::current_path(saved);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the generated header

namespace {

// A small classification and the double-precision results it came from, for emit(): 2 records, a worst class-A comparison in the SWEEP (or, with `published_worst`, in the published cases)
struct Small {
    ms::Classes classes;
    ms::Results dbl;
    double sensitivity[3] = {0.5, 0.625, 0.75};   // three different numbers: the three places of the sentence that states them cannot be told apart by a swap
};

Small small_case(bool published_worst) {
    Small s;
    std::vector<double> p1 = {1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5, 11.5};
    std::vector<double> s2 = {-1.0, 0.0, 1e-30, 4.25, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0};
    s.dbl.set(g7("P", 1), p1);
    s.dbl.set(g7d("P", 1), {0.125});
    s.dbl.set(g7("S", 2), s2);
    s.dbl.set(g7d("S", 2), {0.5});
    const ms::Key worst_key = published_worst ? g7("P", 1) : g7("S", 2);
    s.classes.material = {{0.5, worst_key, 8}, {0.25, g7("P", 3), 2}, {0.125, g7("P", 1), 3}, {0.0625, g7("S", 2), 1}};   // the second entry stands in another place than the first: the sentence names the FIRST
    s.classes.immaterial = {{0.75, g7("P", 1), 4}};
    for (std::size_t k = 1; k <= 7; ++k) s.classes.underflowed.push_back({g7("P", static_cast<long long>(k)), 8, static_cast<double>(k) * 1e-40});
    s.classes.zeros = 3;
    return s;
}

// the paragraph that starts at the comment line beginning `begin`: its lines joined, the "// " dropped, runs of blanks reduced to one
std::string paragraph(const std::vector<std::string>& lines, const std::string& begin, std::vector<std::string>* raw = nullptr) {
    std::string text;
    bool in = false;
    for (const std::string& l : lines) {
        if (!in && l.rfind(begin, 0) == 0) in = true;
        if (in) {
            if (l.empty() || l == "//") break;
            if (l.rfind("//   underflow", 0) == 0) break;
            text += (text.empty() ? "" : " ") + l.substr(3);
            if (raw != nullptr) raw->push_back(l);
        }
    }
    std::string out;
    bool space = false;
    for (const char ch : text) {
        if (ch == ' ') {
            space = true;
            continue;
        }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += ch;
    }
    return out;
}

}  // namespace

TEST_CASE("emit: the header's table, recipe and constants line by line on a small case, the paragraphs unwrapped", "[msis_reference][behaviour]") {
    const Small s = small_case(false);
    const std::string text = ms::emit("FakeFortran 1.0 (test)", "0123abcd", s.classes, s.dbl, s.sensitivity);
    const std::vector<std::string> lines = lines_of(text);
    REQUIRE(lines.size() > 80);
    CHECK(lines.at(0) == "#pragma once");
    CHECK(lines.at(1) == "// msis_reference_values.hpp — GENERATED.  Do not edit.");
    CHECK(lines.at(2) == "//");
    CHECK(lines.at(3) == "// Produced by tools/msis_reference.cpp from the hash-pinned NRL reference");
    CHECK(lines.at(4) == "// implementation NRLMSISE-00.FOR, sha256 0123abcd");
    CHECK(lines.at(5) == "//");
    CHECK(lines.at(6) == "// SPEC-atmosphere ATMO-R-028.  NRL PUBLISHES NO REFERENCE VALUE FOR THIS MODEL:");
    CHECK(lines.at(12) == "// can tell whether the FORTRAN computes what the paper describes.");
    CHECK(lines.at(14) == "// THE RECIPE, so that this file is reproducible rather than merely present:");
    CHECK(lines.at(15) == "//   compiler : FakeFortran 1.0 (test)");
    CHECK(lines.at(16) == "//   flags    : -std=legacy -fdec-char-conversions -O0");
    CHECK(lines.at(17) == "//   promoted : -std=legacy -fdec-char-conversions -O0 -freal-4-real-8   (same source, every REAL widened)");
    CHECK(lines.at(18) == "//   source   : lines 1..2437 of the pinned file; the distribution's own driver");
    CHECK(lines.at(27) == "//              median artefact measured below.");
    CHECK(lines.at(29) == "// THE TOLERANCE IS DERIVED HERE, not asserted elsewhere.  Single precision");
    // the table of classes: counts right-aligned in five columns; median is the element at size / 2 of the sorted list
    CHECK(lines.at(33) == "//   A  material         4   median 1.2500e-01   worst 5.0000e-01");
    CHECK(lines.at(34) == "//   B  immaterial       1                        worst 7.5000e-01");
    CHECK(lines.at(35) == "//   C  underflowed      7   single returned exactly zero, double did not");
    CHECK(lines.at(36) == "//   Z  zero in both     3   the documented zeros below 72.5 km, and anomalous O");
    CHECK(lines.at(37) == "//   total              15   = 2 records x 12 quantities");
    // the paragraphs, unwrapped
    CHECK(paragraph(lines, "// The class boundary is physical") ==
          "The class boundary is physical, not numerical: a species whose mass is less than 1e-15 of the total density cannot affect drag. It is not tuned -- class A's worst is 5.000e-01 at a threshold of 1e-12 and 6.250e-01 "
          "at 1e-15, and only 7.500e-01 at 1e-20.");
    CHECK(paragraph(lines, "// The worst class-A comparison") ==
          "The worst class-A comparison is anomalous O at sweep alt 5 km, condition (172,29000,60,-70,150,150,4) -- a SWEEP point, not one of the 17 published cases (whose own worst is 2.500e-01). THE SWEEP DOES NOT "
          "MERELY FAIL TO LOOSEN THE BOUND HERE, it LOOSENS IT: the 2 further material comparisons it adds found a worse one than any of the 17 published cases did.");
    // no line of a wrapped paragraph is longer than 80 bytes ("// " and at most 77 columns)
    std::vector<std::string> wrapped;
    (void)paragraph(lines, "// The class boundary is physical", &wrapped);
    (void)paragraph(lines, "// The worst class-A comparison", &wrapped);
    CHECK(wrapped.size() >= 6);
    for (const std::string& l : wrapped) CHECK(l.size() <= 80);
    // six underflow lines and a count of the rest
    CHECK(contains(text, "//   underflow: g7 P1 quantity 8: single 0, double 1.000000e-40\n"));
    CHECK(contains(text, "//   underflow: g7 P6 quantity 8: single 0, double 6.000000e-40\n"));
    CHECK_FALSE(contains(text, "//   underflow: g7 P7"));
    CHECK(contains(text, "//   ... and 1 more, all anomalous oxygen below 120 km\n"));
    // the code
    CHECK(contains(text, "\n#include <array>\n#include <cstddef>\n\nnamespace odl::atmosphere::reference {\n\n/// One evaluation: the nine densities, then exospheric and local temperature.\nstruct Record {\n"));
    CHECK(contains(text, "    char    set;        ///< 'P' a published driver case, 'S' a sweep point\n"));
    CHECK(contains(text, "    double  gtd7d_rho;  ///< GTD7D's total mass density, which is NOT gtd7[5]\n};\n"));
    CHECK(contains(text, "inline constexpr double kMaterialFraction     = 1e-15;\n"));
    CHECK(contains(text, "inline constexpr double kSingleMedianRelative = 1.250000e-01;\n"));
    CHECK(contains(text, "inline constexpr double kSingleWorstRelative  = 5.000000e-01;\n"));
    CHECK(contains(text, "inline constexpr std::size_t kClassMaterial    = 4;\n"));
    CHECK(contains(text, "inline constexpr std::size_t kClassImmaterial  = 1;\n"));
    CHECK(contains(text, "inline constexpr std::size_t kClassUnderflowed = 7;\n"));
    CHECK(contains(text, "inline constexpr std::size_t kClassZero        = 3;\n"));
    CHECK(contains(text, "inline constexpr std::size_t kPublishedCases = 1;\ninline constexpr std::size_t kSweepCases     = 1;\n"));
    CHECK(contains(text, "inline constexpr std::array<Record, 2> kValues{{\n"));
    // the two records, 17 digits after the point (the C library's %.17e is Python's {x:.17e}); the first row also literally, for a value that is exact in binary
    const auto row_text = [](char set_char, long long index, const std::vector<double>& v, double rho) {
        std::string nums;
        for (std::size_t i = 0; i < v.size(); ++i) nums += (i != 0 ? ", " : "") + fmt("%.17e", v.at(i));
        return std::string("    {'") + set_char + "', " + pad3(index) + ", {" + nums + "}, " + fmt("%.17e", rho) + "},\n";
    };
    CHECK(contains(text, row_text('P', 1, *s.dbl.find(g7("P", 1)), 0.125)));
    CHECK(contains(text, row_text('S', 2, *s.dbl.find(g7("S", 2)), 0.5)));
    CHECK(contains(text,
                   "    {'P',   1, {1.50000000000000000e+00, 2.50000000000000000e+00, 3.50000000000000000e+00, 4.50000000000000000e+00, 5.50000000000000000e+00, 6.50000000000000000e+00, "
                   "7.50000000000000000e+00, 8.50000000000000000e+00, 9.50000000000000000e+00, 1.05000000000000000e+01, 1.15000000000000000e+01}, 1.25000000000000000e-01},\n"));
    CHECK(contains(text, "{-1.00000000000000000e+00, 0.00000000000000000e+00, "));
    CHECK(contains(text, "}};\n\n}  // namespace odl::atmosphere::reference\n"));
    CHECK(text.back() == '\n');
    // the rows come in the order of the keys, whatever the order the results were set in
    ms::Results reordered;
    reordered.set(g7("S", 2), s.dbl.find(g7("S", 2)) != nullptr ? *s.dbl.find(g7("S", 2)) : std::vector<double>{});
    reordered.set(g7d("S", 2), {0.5});
    reordered.set(g7d("P", 1), {0.125});
    reordered.set(g7("P", 1), *s.dbl.find(g7("P", 1)));
    CHECK(ms::emit("FakeFortran 1.0 (test)", "0123abcd", s.classes, reordered, s.sensitivity) == text);
    // the sweep's index is padded to three columns, the published cases' likewise
    ms::Results big;
    big.set(g7("S", 164), *s.dbl.find(g7("S", 2)));
    big.set(g7d("S", 164), {0.5});
    CHECK(contains(ms::emit("c", "s", s.classes, big, s.sensitivity), "    {'S', 164, {"));
}

TEST_CASE("emit: when the worst class-A comparison is one of the published cases; and the refusals", "[msis_reference][behaviour]") {
    const Small s = small_case(true);
    const std::string text = ms::emit("FakeFortran 1.0 (test)", "0123abcd", s.classes, s.dbl, s.sensitivity);
    const std::vector<std::string> lines = lines_of(text);
    // here the worst, 0.5 at (P1, quantity 8), is a published case, so the sweep adds no worse one: the further material comparisons are those of the sweep (mat.size() - published) = 1
    CHECK(paragraph(lines, "// The worst class-A comparison") ==
          "The worst class-A comparison is anomalous O at published case 1 -- one of the 17 published cases: the 1 further material comparisons the sweep adds, across every branch boundary, do NOT exceed it.");
    // a quantity the table of names does not know is named by its number; one it knows is named
    ms::Classes odd = s.classes;
    odd.material.front().quantity = 11;
    const std::string odd_text = ms::emit("c", "s", odd, s.dbl, s.sensitivity);
    CHECK(contains(odd_text, "The worst class-A comparison is quantity 11 at published case 1"));
    for (std::size_t q = 0; q < kSpecies.size(); ++q) {
        ms::Classes named = s.classes;
        named.material.front().quantity = q;
        CHECK(paragraph(lines_of(ms::emit("c", "s", named, s.dbl, s.sensitivity)), "// The worst class-A comparison").rfind("The worst class-A comparison is " + kSpecies.at(q) + " at published case 1", 0) == 0);
    }
    // the median is the element at size / 2 of the sorted list: five entries -> the third
    ms::Classes five = s.classes;
    five.material = {{0.9, g7("P", 1), 0}, {0.8, g7("P", 1), 1}, {0.7, g7("P", 1), 2}, {0.6, g7("P", 1), 3}, {0.5, g7("P", 1), 4}};
    CHECK(contains(ms::emit("c", "s", five, s.dbl, s.sensitivity), "median 7.0000e-01   worst 9.0000e-01"));
    CHECK(contains(ms::emit("c", "s", five, s.dbl, s.sensitivity), "kSingleMedianRelative = 7.000000e-01;"));
    five.material.resize(1);
    CHECK(contains(ms::emit("c", "s", five, s.dbl, s.sensitivity), "median 9.0000e-01   worst 9.0000e-01"));   // one entry: index 0
    // no underflow at all, or exactly six: no "... and" line; seven: one
    ms::Classes none = s.classes;
    none.underflowed.clear();
    CHECK_FALSE(contains(ms::emit("c", "s", none, s.dbl, s.sensitivity), "//   underflow:"));
    ms::Classes six = s.classes;
    six.underflowed.resize(6);
    const std::string six_text = ms::emit("c", "s", six, s.dbl, s.sensitivity);
    CHECK(contains(six_text, "//   underflow: g7 P6 quantity 8"));
    CHECK_FALSE(contains(six_text, "... and"));
    ms::Classes twelve = s.classes;
    for (long long k = 8; k <= 20; ++k) twelve.underflowed.push_back({g7("P", k), 8, 1e-40});
    CHECK(contains(ms::emit("c", "s", twelve, s.dbl, s.sensitivity), "//   ... and 14 more, all anomalous oxygen below 120 km\n"));
    // a classification without a material or without an immaterial comparison has no worst case to state: refused
    const auto message_of = [&](const ms::Classes& c, const ms::Results& d) {
        try {
            (void)ms::emit("c", "s", c, d, s.sensitivity);
        } catch (const ms::BadInput& e) {
            return std::string(e.what());
        }
        return std::string("(nothing was thrown)");
    };
    ms::Classes no_material = s.classes;
    no_material.material.clear();
    CHECK(message_of(no_material, s.dbl) == "the classification has no material or no immaterial comparison: the header would have no worst case to state");
    ms::Classes no_immaterial = s.classes;
    no_immaterial.immaterial.clear();
    CHECK(message_of(no_immaterial, s.dbl) == "the classification has no material or no immaterial comparison: the header would have no worst case to state");
    // a g7 record with no GTD7D partner: refused with the key
    ms::Results lonely;
    lonely.set(g7("P", 1), *s.dbl.find(g7("P", 1)));
    lonely.set(g7("S", 2), *s.dbl.find(g7("S", 2)));
    lonely.set(g7d("P", 1), {0.125});
    CHECK(message_of(s.classes, lonely) == "no GTD7D record for S2");
    ms::Results empty_rho;
    empty_rho.set(g7("P", 1), *s.dbl.find(g7("P", 1)));
    empty_rho.set(g7d("P", 1), {});
    CHECK(message_of(s.classes, empty_rho) == "no GTD7D record for P1");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

TEST_CASE("run_on: the command line", "[msis_reference][behaviour]") {
    ms::Settings settings;
    settings.root = "/nonexistent-root-for-the-argument-tests";
    settings.path = "";   // no compiler anywhere: a run that gets past the arguments says so and stops
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_with(settings, {flag});
        CHECK(r.code == 0);
        CHECK(r.err.empty());
        CHECK(r.out.rfind(kUsage, 0) == 0);
        CHECK(r.out == std::string(kUsage) + kHelp);
        CHECK(contains(r.out, "Freeze the NRLMSISE-00 reference implementation's output (SPEC-atmosphere ATMO-R-028)"));
        CHECK(contains(r.out, "\n  --work WORK                where the Fortran is built and run (default: tools/.msisref of the tree, git-ignored)\n"));
        CHECK(contains(r.out, "\n  --sweep-input SWEEP_INPUT  also write the standard input of the sweep to this file\n"));
        CHECK(contains(r.out, "\n  --check                    regenerate and compare, do not write: exit 1 unless the committed header is exactly what the regeneration emits\n"));
        CHECK(contains(r.out, "\nexit codes: 0 written / the committed header reproduces / no compiler   1 it differs, or a compiler or a reference run failed, or the cached bytes are not the pinned bytes\n"
                              "            2 an argument error, or a file that is missing or cannot be read   70 an error the tool did not anticipate\n"));
        CHECK(r.out.back() == '\n');
    }
    CHECK(run_with(settings, {"--check", "-h"}).code == 0);
    const Result none = run_with(settings, {});
    CHECK(none.code == 0);
    CHECK(none.out == "no Fortran compiler on PATH; nothing regenerated (ATMO-R-028: the gate does not need one)\n");
    CHECK(none.err.empty());
    for (const std::string& bad : {std::string("--bogus"), std::string("x"), std::string("-c"), std::string("--chec"), std::string("--check=1"), std::string("--wor=x"), std::string("--sweep")}) {
        const Result r = run_with(settings, {bad});
        INFO(bad);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "msis_reference: error: unrecognized arguments: " + bad + "\n");
    }
    for (const char* opt : {"--root", "--out", "--work", "--sweep-input"}) {
        const Result at_end = run_with(settings, {opt});
        CHECK(at_end.code == 2);
        CHECK(at_end.err == std::string(kUsage) + "msis_reference: error: argument " + opt + ": expected one argument\n");
        const Result before = run_with(settings, {opt, "--check"});
        CHECK(before.code == 2);
        CHECK(before.err == std::string(kUsage) + "msis_reference: error: argument " + opt + ": expected one argument\n");
        // only "--" starts an option: a following word that begins with three hyphens is one too
        const Result triple = run_with(settings, {opt, "---x"});
        CHECK(triple.code == 2);
        CHECK(triple.err == std::string(kUsage) + "msis_reference: error: argument " + opt + ": expected one argument\n");
    }
    // the options accepted: every one in both forms, no compiler -> the same message and exit 0
    CHECK(run_with(settings, {"--root", "x", "--out=y", "--work", "z", "--sweep-input=w", "--check"}).out == none.out);
    CHECK(ms::default_settings().root == fs::path(ODL_TREE_ROOT));
    CHECK(ms::default_root() == fs::path(ODL_TREE_ROOT));
    CHECK(ms::default_settings().compilers == std::vector<std::string>{"gfortran", "gfortran-16", "gfortran-15", "gfortran-14", "gfortran-13"});
    CHECK_FALSE(ms::default_settings().path.has_value());
    CHECK(ms::default_settings().work.empty());
    CHECK(std::string(ms::kTool) == "msis_reference");
    CHECK(std::string(ms::kSourceId) == "nrlmsise00-fortran");
    CHECK(std::string(ms::kDefaultOut) == "modules/atmosphere/src/msis_reference_values.hpp");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the whole tool through the stand-in compiler

namespace {


// the row of the generated file for one record, written with the C library's  %.17e
std::string value_row(const Rec& r) {
    std::string nums;
    for (std::size_t i = 0; i < r.d.size(); ++i) nums += (i != 0 ? ", " : "") + fmt("%.17e", r.d.at(i));
    return std::string("    {'") + r.set + "', " + pad3(r.index) + ", {" + nums + "}, " + fmt("%.17e", r.drho) + "},";
}

std::size_t number_after(const std::string& text, const std::string& label) {
    const std::size_t at = text.find(label);
    REQUIRE(at != std::string::npos);
    return std::strtoull(text.c_str() + at + label.size(), nullptr, 10);
}

}  // namespace

// The stand-in reference is a program of the tests, and its reading of standard input was rewritten (it is the one place in the tree that read a stream to its end through std::istreambuf_iterator, which GCC 13.3 --
// GitHub's runner -- warns about under -Werror; PROVENANCE.md section 41.22): the bytes it saw must be the bytes it was fed, at the sizes where a block or a pipe ends.
TEST_CASE("the stand-in reference reads all of its standard input -- every byte, at the sizes where a block or a pipe ends", "[msis_reference][behaviour]") {
    const fs::path stand_in = fakebin() / "odl_fake_reference";
    REQUIRE(fs::exists(stand_in));
    TempDir td{"odl-mr-stdin"};
    const fs::path program = td.path() / "ref_s";   // it behaves by its own name: REF_S
    fs::copy_file(stand_in, program);
    const fs::path seen = td.path() / "seen.bin";
    EnvScope env;
    for (const char* steering : {"ODL_FAKE_REF_OUT_REF_S", "ODL_FAKE_REF_EXIT_REF_S", "ODL_FAKE_REF_STDERR_REF_S"}) env.unset(steering);
    env.set("ODL_FAKE_REF_STDIN_REF_S", seen.string());

    // Bytes with no structure that a text reader would forgive: all 256 values (NUL, CR, LF, Ctrl-Z, the high half) in an order that shifts by one every 251 bytes, so that a block lost, repeated, moved or cut cannot read back as the input.
    const auto pattern = [](std::size_t n) {
        std::string s(n, '\0');
        for (std::size_t i = 0; i < n; ++i) s.at(i) = static_cast<char>((i * 7 + i / 251) % 256);
        return s;
    };
    // 8192 is the stand-in's block and 65536 a pipe's capacity on Linux: one byte either side of each, the empty input, one byte, two blocks exactly, and 200,003 bytes (three pipes' worth and more, a multiple of nothing)
    for (const std::size_t n : std::vector<std::size_t>{0, 1, 2, 8191, 8192, 8193, 16384, 65535, 65536, 65537, 200003}) {
        const std::string input = pattern(n);
        INFO("standard input of " << n << " bytes");
        fs::remove(seen);
        ProcessOptions options;
        options.input = input;
        const ProcessResult r = run_process({program.string()}, options);
        CHECK(r.exit_code == 0);
        CHECK(r.out.empty());
        CHECK(r.err.empty());
        REQUIRE(fs::exists(seen));
        const Bytes got = read_bytes(seen);
        CHECK(got.size() == n);
        const bool same = as_text(got) == input;
        CHECK(same);
    }
}

TEST_CASE("run_on: the whole tool through the stand-in compiler -- the build, the runs, the classification, the header", "[msis_reference][behaviour]") {
    const Tree t;
    const Result r = t.run();
    REQUIRE(r.code == 0);
    CHECK(r.err.empty());
    const Brute b = brute_classify(t.recs, 1e-15);
    REQUIRE(b.material.size() + b.immaterial.size() + b.underflowed + b.zeros == 2172);   // 181 records x 12 quantities
    CHECK(b.material.size() > 1000);
    CHECK(b.immaterial.size() > 5);
    CHECK(b.underflowed > 5);
    CHECK(r.out == "wrote modules/atmosphere/src/msis_reference_values.hpp: 181 reference records; comparisons material " + std::to_string(b.material.size()) + ", immaterial " + std::to_string(b.immaterial.size()) +
                       ", underflowed " + std::to_string(b.underflowed) + ", zero " + std::to_string(b.zeros) + "\n");

    // what the compiler was asked: --version, then the build as published, then every REAL widened
    const std::vector<std::vector<std::string>> calls = t.calls();
    REQUIRE(calls.size() == 3);
    CHECK(calls.at(0) == std::vector<std::string>{"--version"});
    CHECK(calls.at(1) == std::vector<std::string>{"-std=legacy", "-fdec-char-conversions", "-O0", "-o", (t.work() / "ref_s").string(), (t.work() / "model.f").string(), (t.work() / "driver.f").string()});
    CHECK(calls.at(2) == std::vector<std::string>{"-std=legacy", "-fdec-char-conversions", "-O0", "-freal-4-real-8", "-o", (t.work() / "ref_d").string(), (t.work() / "model.f").string(), (t.work() / "driver.f").string()});
    // the sources it compiled: the first 2,437 lines of the cached Fortran, and the tool's driver; the same for both builds
    for (const char* suffix : {".s", ".d"}) {
        CHECK(read_text(t.scratch / "capture" / (std::string("model.f") + suffix)) == synthetic_fortran(2437));
        CHECK(read_text(t.scratch / "capture" / (std::string("driver.f") + suffix)) == ms::driver_source());
    }
    // what the references were fed: the sweep, on standard input, twice
    CHECK(read_text(t.scratch / "stdin_s.txt") == ms::sweep_input());
    CHECK(read_text(t.scratch / "stdin_d.txt") == ms::sweep_input());
    // the work directory is tools/.msisref of the tree (made on the way), holding the sources and the two executables
    CHECK(fs::exists(t.work() / "model.f"));
    CHECK(fs::exists(t.work() / "driver.f"));
    CHECK(fs::exists(t.work() / "ref_s"));
    CHECK(fs::exists(t.work() / "ref_d"));
    CHECK(read_text(t.work() / "model.f") == synthetic_fortran(2437));
    CHECK(read_text(t.work() / "driver.f") == ms::driver_source());

    // the header: the hash of the cached bytes, the compiler's first line, the table, the paragraphs, the constants, the rows
    const std::string text = read_text(t.header());
    const std::vector<std::string> lines = lines_of(text);
    CHECK(contains(text, "// implementation NRLMSISE-00.FOR, sha256 " + t.sha + "\n"));
    CHECK(contains(text, "//   compiler : GNU Fortran (Fake) 99.9.9 20990101\n"));
    const auto& m = b.material;
    const double median = std::get<0>(m.at(m.size() / 2));
    const double worst = std::get<0>(m.at(0));
    CHECK(contains(text, "//   A  material     " + pad5(m.size()) + "   median " + fmt("%.4e", median) + "   worst " + fmt("%.4e", worst) + "\n"));
    CHECK(contains(text, "//   B  immaterial   " + pad5(b.immaterial.size()) + "                        worst " + fmt("%.4e", std::get<0>(b.immaterial.at(0))) + "\n"));
    CHECK(contains(text, "//   C  underflowed  " + pad5(b.underflowed) + "   single returned exactly zero, double did not\n"));
    CHECK(contains(text, "//   Z  zero in both " + pad5(b.zeros) + "   the documented zeros below 72.5 km, and anomalous O\n"));
    CHECK(contains(text, "//   total           " + pad5(2172) + "   = 181 records x 12 quantities\n"));
    // the sensitivity of class A's worst to the materiality threshold, measured three times over the same numbers
    std::array<std::string, 3> sens;
    const double thresholds[3] = {1e-12, 1e-15, 1e-20};
    for (std::size_t k = 0; k < 3; ++k) {
        const Brute at = brute_classify(t.recs, thresholds[k]);
        REQUIRE_FALSE(at.material.empty());
        sens.at(k) = fmt("%.3e", std::get<0>(at.material.at(0)));
    }
    CHECK(sens.at(2) != sens.at(1));   // the immaterial argon (r = 8/1024) joins class A at 1e-20: the stand-in's numbers make the threshold matter
    CHECK(paragraph(lines, "// The class boundary is physical") ==
          "The class boundary is physical, not numerical: a species whose mass is less than 1e-15 of the total density cannot affect drag. It is not tuned -- class A's worst is " + sens.at(0) + " at a threshold of 1e-12 and " +
              sens.at(1) + " at 1e-15, and only " + sens.at(2) + " at 1e-20.");
    // the worst class-A comparison: the largest (r, kind, set, index, quantity) of the tuples
    const auto& w = m.at(0);
    const std::string set = std::get<2>(w);
    std::size_t published = 0;
    double worst_published = 0.0;
    for (const auto& e : m) {
        if (std::get<2>(e) == "P") {
            ++published;
            worst_published = std::max(worst_published, std::get<0>(e));
        }
    }
    const std::string species = std::get<4>(w) < kSpecies.size() ? kSpecies.at(std::get<4>(w)) : "quantity " + std::to_string(std::get<4>(w));
    const std::string head = "The worst class-A comparison is " + species + " at " + place_of(set, std::get<3>(w)) + " -- ";
    if (set == "P") {
        CHECK(paragraph(lines, "// The worst class-A comparison") ==
              head + "one of the 17 published cases: the " + std::to_string(m.size() - published) + " further material comparisons the sweep adds, across every branch boundary, do NOT exceed it.");
    } else {
        CHECK(paragraph(lines, "// The worst class-A comparison") ==
              head + "a SWEEP point, not one of the 17 published cases (whose own worst is " + fmt("%.3e", worst_published) + "). THE SWEEP DOES NOT MERELY FAIL TO LOOSEN THE BOUND HERE, it LOOSENS IT: the " +
                  std::to_string(m.size() - published) + " further material comparisons it adds found a worse one than any of the 17 published cases did.");
    }
    // the first six underflows in the order the references printed them, and the count of the others
    for (std::size_t k = 0; k < 6; ++k) {
        const auto& u = b.underflows.at(k);
        CHECK(contains(text, "//   underflow: " + std::get<0>(u) + " " + std::get<1>(u) + std::to_string(std::get<2>(u)) + " quantity " + std::to_string(std::get<3>(u)) + ": single 0, double " + fmt("%.6e", std::get<4>(u)) + "\n"));
    }
    CHECK(contains(text, "//   ... and " + std::to_string(b.underflowed - 6) + " more, all anomalous oxygen below 120 km\n"));
    // the constants and the records
    CHECK(contains(text, "inline constexpr double kSingleMedianRelative = " + fmt("%.6e", median) + ";\n"));
    CHECK(contains(text, "inline constexpr double kSingleWorstRelative  = " + fmt("%.6e", worst) + ";\n"));
    CHECK(number_after(text, "kClassMaterial    = ") == m.size());
    CHECK(number_after(text, "kClassImmaterial  = ") == b.immaterial.size());
    CHECK(number_after(text, "kClassUnderflowed = ") == b.underflowed);
    CHECK(number_after(text, "kClassZero        = ") == b.zeros);
    CHECK(number_after(text, "kPublishedCases = ") == 17);
    CHECK(number_after(text, "kSweepCases     = ") == 164);
    CHECK(contains(text, "inline constexpr std::array<Record, 181> kValues{{\n"));
    for (std::size_t k = 0; k < t.recs.size(); ++k) CHECK(contains(text, value_row(t.recs.at(k)) + "\n"));
    // the rows are in the order of the keys: the published cases, then the sweep
    std::vector<std::string> rows;
    for (const std::string& l : lines) {
        if (l.rfind("    {'", 0) == 0) rows.push_back(l);
    }
    REQUIRE(rows.size() == 181);
    CHECK(rows.front() == value_row(t.recs.front()));
    CHECK(rows.at(16) == value_row(t.recs.at(16)));
    CHECK(rows.at(17) == value_row(t.recs.at(17)));
    CHECK(rows.back() == value_row(t.recs.back()));
}

TEST_CASE("run_on: --check, --work, --sweep-input, --out, --root and the choice of compiler", "[msis_reference][behaviour]") {
    const Tree t;
    // nothing written yet: the header does not exist, so it differs
    const Result missing = t.run({"--check"});
    CHECK(missing.code == 1);
    CHECK(missing.out.empty());
    CHECK(missing.err == "REGENERATED OUTPUT DIFFERS from the committed file\n");
    CHECK_FALSE(fs::exists(t.header()));   // a check writes nothing
    REQUIRE(t.run().code == 0);
    const std::string expected = read_text(t.header());
    const Result ok = t.run({"--check"});
    CHECK(ok.code == 0);
    CHECK(ok.err.empty());
    CHECK(ok.out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran\n");
    // CRLF line ends in the committed file are read as Python's read_text() reads them
    {
        std::string crlf;
        for (const char c : expected) {
            if (c == '\n') crlf += '\r';
            crlf += c;
        }
        write_text(t.header(), crlf);
        CHECK(t.run({"--check"}).code == 0);
    }
    for (const std::string& changed : {expected + "\n", expected.substr(0, expected.size() - 1), std::string(), std::string("x") + expected}) {
        write_text(t.header(), changed);
        const Result r = t.run({"--check"});
        CHECK(r.code == 1);
        CHECK(r.err == "REGENERATED OUTPUT DIFFERS from the committed file\n");
        CHECK(r.out.empty());
    }
    // the check writes nothing, the write replaces
    write_text(t.header(), "old");
    CHECK(t.run({"--check"}).code == 1);
    CHECK(read_text(t.header()) == "old");
    CHECK(t.run().code == 0);
    CHECK(read_text(t.header()) == expected);

    // --out: relative to the tree, in both forms; the report names it as given
    const Result other = t.run({"--out", "modules/atmosphere/src/other.hpp"});
    CHECK(other.code == 0);
    CHECK(contains(other.out, "wrote modules/atmosphere/src/other.hpp: 181 reference records;"));
    CHECK(read_text(t.root / "modules" / "atmosphere" / "src" / "other.hpp") == expected);
    CHECK(t.run({"--out=modules/atmosphere/src/third.hpp"}).code == 0);
    CHECK(fs::exists(t.root / "modules" / "atmosphere" / "src" / "third.hpp"));
    const Result other_check = t.run({"--check", "--out", "modules/atmosphere/src/other.hpp"});
    CHECK(other_check.out == "ok  modules/atmosphere/src/other.hpp reproduces from nrlmsise00-fortran with gfortran\n");
    // --work: where the Fortran is built and run
    const fs::path work = t.root / "elsewhere" / "w";
    CHECK(t.run({"--work", work.string()}).code == 0);
    CHECK(fs::exists(work / "ref_s"));
    CHECK(fs::exists(work / "ref_d"));
    CHECK(fs::exists(work / "model.f"));
    CHECK(t.run({"--work=" + (t.root / "w2").string()}).code == 0);
    CHECK(fs::exists(t.root / "w2" / "ref_d"));
    // --sweep-input: the standard input of the sweep, also written there
    const fs::path sweep = t.root / "sweep.txt";
    CHECK(t.run({"--sweep-input", sweep.string()}).code == 0);
    CHECK(read_text(sweep) == ms::sweep_input());
    CHECK(t.run({"--sweep-input=" + (t.root / "sweep2.txt").string()}).code == 0);
    CHECK(read_text(t.root / "sweep2.txt") == ms::sweep_input());
    // --root: the settings' root is replaced
    {
        ms::Settings s = t.settings();
        s.root = "/nonexistent-root";
        const Result r = run_with(s, {"--root", t.root.string(), "--check"});
        CHECK(r.code == 0);
        CHECK(run_with(s, {"--check"}).code == 2);   // the settings' own root has no manifest
    }
    // the settings' work directory is used when --work is not given
    {
        ms::Settings s = t.settings();
        s.work = t.root / "settings-work";
        CHECK(run_with(s, {}).code == 0);
        CHECK(fs::exists(t.root / "settings-work" / "ref_s"));
    }
    // the compiler is the first of  gfortran, gfortran-16, -15, -14, -13  found on PATH, and the message names it
    fs::remove(t.bin / "gfortran");
    CHECK(t.run({"--check"}).out == "no Fortran compiler on PATH; nothing regenerated (ATMO-R-028: the gate does not need one)\n");
    t.install("gfortran-13");
    CHECK(t.run({"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran-13\n");
    t.install("gfortran-14");
    CHECK(t.run({"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran-14\n");
    t.install("gfortran-15");
    CHECK(t.run({"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran-15\n");
    t.install("gfortran-16");
    CHECK(t.run({"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran-16\n");
    t.install("gfortran");
    CHECK(t.run({"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran\n");
    // a compiler the settings do not list is not found
    ms::Settings only = t.settings();
    only.compilers = {"gfortran-99"};
    CHECK(run_with(only, {"--check"}).out == "no Fortran compiler on PATH; nothing regenerated (ATMO-R-028: the gate does not need one)\n");
    t.install("gfortran-99");
    CHECK(run_with(only, {"--check"}).out == "ok  modules/atmosphere/src/msis_reference_values.hpp reproduces from nrlmsise00-fortran with gfortran-99\n");
    // no compiler: nothing is read, written or made
    {
        ms::Settings none = t.settings();
        none.path = (t.root / "empty-bin").string();
        none.work = t.root / "never";
        const Result r = run_with(none, {"--sweep-input", (t.root / "never.txt").string()});
        CHECK(r.code == 0);
        CHECK(r.out == "no Fortran compiler on PATH; nothing regenerated (ATMO-R-028: the gate does not need one)\n");
        CHECK_FALSE(fs::exists(t.root / "never"));
        CHECK_FALSE(fs::exists(t.root / "never.txt"));
    }
}

TEST_CASE("run_on: every refusal -- the manifest, the cache, the compiler, the references and their numbers", "[msis_reference][behaviour]") {
    const std::string mp = "msis_reference: ";
    // the cached bytes are not the pinned bytes: exit 1, three lines (the Python's sys.exit(message))
    {
        Tree t;
        write_text(t.root / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR", t.fortran + "x");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == "nrlmsise00-fortran: cached bytes are not the pinned bytes\n  want " + t.sha + "\n  got  " + sha256_hex(as_bytes(t.fortran + "x")) + "\n");
        CHECK_FALSE(fs::exists(t.header()));
    }
    // the manifest: missing, not JSON, wrong shape
    {
        Tree t;
        fs::remove(t.root / "manifest" / "manifest.json");
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err.rfind(mp, 0) == 0);
        CHECK(contains(r.err, (t.root / "manifest" / "manifest.json").string()));
        write_text(t.root / "manifest" / "manifest.json", "{");
        CHECK(t.run().err == mp + (t.root / "manifest" / "manifest.json").string() + ": Expecting property name enclosed in double quotes: line 1 column 2\n");
        const auto with_manifest = [&](const std::string& json) {
            write_text(t.root / "manifest" / "manifest.json", json);
            return t.run();
        };
        CHECK(contains(with_manifest("{\"entries\": []}").err, "no \"entries\" list or no \"cache\" string\n"));
        CHECK(contains(with_manifest("{\"cache\": \"cache\"}").err, "no \"entries\" list or no \"cache\" string\n"));                       // no entries at all
        CHECK(contains(with_manifest("{\"entries\": \"x\", \"cache\": \"cache\"}").err, "no \"entries\" list or no \"cache\" string\n"));   // entries that are no list
        CHECK(contains(with_manifest("{\"entries\": [], \"cache\": 5}").err, "no \"entries\" list or no \"cache\" string\n"));              // a cache that is no string
        CHECK(contains(with_manifest("{\"entries\": [{\"filename\": \"x\"}], \"cache\": \"cache\"}").err, ": an entry without an id\n"));
        CHECK(contains(with_manifest("{\"entries\": [{\"id\": \"other\"}], \"cache\": \"cache\"}").err, ": no entry nrlmsise00-fortran\n"));
        const Result no_hash = with_manifest("{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"filename\": \"x\"}], \"cache\": \"cache\"}");
        CHECK(no_hash.code == 2);
        CHECK(no_hash.err == mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n");
        // ... or either of the two that is no string
        const std::string id_part = "{\"entries\": [{\"id\": \"nrlmsise00-fortran\", ";
        const std::string no_fields = mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n";
        CHECK(with_manifest(id_part + "\"sha256\": \"ab\"}], \"cache\": \"cache\"}").err == no_fields);                       // no filename
        CHECK(with_manifest(id_part + "\"filename\": 5, \"sha256\": \"ab\"}], \"cache\": \"cache\"}").err == no_fields);      // a filename that is no string
        CHECK(with_manifest(id_part + "\"filename\": \"x\", \"sha256\": 7}], \"cache\": \"cache\"}").err == no_fields);        // a hash that is no string
    }
    // the cached file is missing
    {
        Tree t;
        fs::remove(t.root / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR");
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(contains(r.err, "NRLMSISE-00.FOR"));
    }
    // the compiler fails: exit 1 and the tail of its standard error (2,000 characters at most)
    {
        Tree t;
        t.env.set("ODL_FAKE_GFORTRAN_EXIT", "1");
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", "boom");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.err == "gfortran failed:\nboom\n\n");
        CHECK_FALSE(fs::exists(t.header()));
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", "BEGIN" + std::string(3000, 'x') + "END");
        CHECK(t.run().err == "gfortran failed:\n" + std::string(1996, 'x') + "END\n\n");
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", std::string(1995, 'y') + "END");   // 1998 bytes and the newline of the stand-in: exactly 1999, all kept
        CHECK(t.run().err == "gfortran failed:\n" + std::string(1995, 'y') + "END\n\n");
        // the tail is counted in CHARACTERS, as the Python's  err[-2000:]  of the decoded text: 1,999 two-byte letters and the stand-in's newline are 2,000 characters (3,999 bytes) and all are kept; one more
        // letter and the first goes (a gfortran in a UTF-8 locale quotes with three-byte quotation marks)
        const auto repeat = [](const std::string& unit, std::size_t times) {
            std::string out;
            for (std::size_t i = 0; i < times; ++i) out += unit;
            return out;
        };
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", repeat("\xC3\xA9", 1999));
        CHECK(t.run().err == "gfortran failed:\n" + repeat("\xC3\xA9", 1999) + "\n\n");
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", repeat("\xC3\xA9", 2000));
        CHECK(t.run().err == "gfortran failed:\n" + repeat("\xC3\xA9", 1999) + "\n\n");
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", "ab" + repeat("\xE2\x80\x98", 1998) + "c");   // 'a', 'b', 1,998 three-byte quotes, 'c' and the newline: 2,002 characters, the first two go
        CHECK(t.run().err == "gfortran failed:\n" + repeat("\xE2\x80\x98", 1998) + "c\n\n");
        t.env.set("ODL_FAKE_GFORTRAN_EXIT", "3");
        t.env.set("ODL_FAKE_GFORTRAN_STDERR", "");
        t.env.unset("ODL_FAKE_GFORTRAN_STDERR");
        CHECK(t.run().err == "gfortran failed:\n\n");   // no message at all
    }
    // a reference run fails (the first, or the second): exit 1 and the tail of its standard error (1,000 characters at most)
    {
        Tree t;
        t.env.set("ODL_FAKE_REF_EXIT_REF_S", "3");
        t.env.set("ODL_FAKE_REF_STDERR_REF_S", "oops");
        const Result r = t.run();
        CHECK(r.code == 1);
        CHECK(r.err == "reference run failed: oops\n\n");
        t.env.set("ODL_FAKE_REF_STDERR_REF_S", "BEGIN" + std::string(2000, 'x') + "END");
        CHECK(t.run().err == "reference run failed: " + std::string(996, 'x') + "END\n\n");
        // characters here too: 999 two-byte letters and the newline are 1,000 characters, all kept; one more and the first goes
        std::string letters;
        for (std::size_t i = 0; i < 999; ++i) letters += "\xC3\xA9";
        t.env.set("ODL_FAKE_REF_STDERR_REF_S", letters);
        CHECK(t.run().err == "reference run failed: " + letters + "\n\n");
        t.env.set("ODL_FAKE_REF_STDERR_REF_S", "\xC3\xA9" + letters);
        CHECK(t.run().err == "reference run failed: " + letters + "\n\n");
        t.env.unset("ODL_FAKE_REF_EXIT_REF_S");
        t.env.set("ODL_FAKE_REF_EXIT_REF_D", "4");
        t.env.set("ODL_FAKE_REF_STDERR_REF_D", "second");
        CHECK(t.run().err == "reference run failed: second\n\n");
    }
    // the "executable" cannot be started: exit 2 naming it
    {
        Tree t;
        t.env.set("ODL_FAKE_GFORTRAN_EXE_MODE", "644");
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err.rfind(mp + "cannot run " + (t.work() / "ref_s").string() + ": ", 0) == 0);
    }
    // the compiler cannot be started either (a file of that name that is not a program)
    {
        Tree t;
        write_text(t.bin / "gfortran", "#!/nonexistent/interpreter\n");   // executable, but its interpreter is not there: the spawn fails
        ::chmod((t.bin / "gfortran").c_str(), 0755);
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err.rfind(mp + "cannot run " + (t.bin / "gfortran").string() + ": ", 0) == 0);
    }
    // the output of a reference is not what the tool reads
    {
        Tree t;
        write_text(t.scratch / "single.txt", "G7 P x 1.0\n");
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err == mp + "the index in a line of the reference's output is not an integer: G7 P x 1.0\n");
        write_text(t.scratch / "single.txt", "G7 P 1 abc\n");
        CHECK(t.run().err == mp + "a number in a line of the reference's output does not parse: abc\n");
        write_text(t.scratch / "single.txt", "G7\n");
        CHECK(t.run().err == mp + "a line of the reference's output is too short: G7\n");
    }
    // the single-precision run lacks a record the double-precision run has
    {
        Tree t;
        const std::vector<std::string> all = lines_of(reference_text(t.recs, true));
        std::string head;
        for (std::size_t i = 0; i < 4; ++i) head += all.at(i) + "\n";   // P1 and P2, both lines each
        write_text(t.scratch / "single.txt", head);
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err == mp + "the single-precision run has no record g7 P3\n");
    }
    // nothing material at the first threshold: the reference printed zeros only
    {
        Tree t;
        const std::string zeros = "G7 P    1 0 0 0 0 0 0 0 0 0 0 0\nG7D P    1 0\n";
        write_text(t.scratch / "single.txt", zeros);
        write_text(t.scratch / "double.txt", zeros);
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err == mp + "no material comparison at the threshold 1e-12\n");
    }
    // everything material and nothing immaterial: no worst case of class B to state
    {
        Tree t;
        const std::string d = "G7 P    1 1 1 1 1 1 1e-20 1 0 0 1 1\nG7D P    1 1\n";
        const std::string s = "G7 P    1 2 2 2 2 2 1e-20 2 0 0 2 2\nG7D P    1 2\n";
        write_text(t.scratch / "single.txt", s);
        write_text(t.scratch / "double.txt", d);
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err == mp + "the classification has no material or no immaterial comparison: the header would have no worst case to state\n");
        CHECK_FALSE(fs::exists(t.header()));
    }
    // the work directory cannot be made (a file stands where a directory should be): a refusal naming it, exit 2 -- the exit 70 of an unanticipated error has no input that reaches it
    {
        Tree t;
        ms::Settings s = t.settings();
        write_text(t.root / "afile", "x");
        s.work = t.root / "afile" / "below";   // a file where a directory should be: create_directories reports it
        const Result r = run_with(s, {});
        CHECK(r.code == 2);
        CHECK(r.err.rfind(mp, 0) == 0);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real tree

namespace {

fs::path tree_path(const char* relative) { return fs::path(ODL_TREE_ROOT) / relative; }
fs::path real_fortran() { return tree_path("data/cache/nrlmsise00-fortran/NRLMSISE-00.FOR"); }

std::string trim_right(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}
std::string squeeze(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (c != ' ' && c != '\t') out += c;
    }
    return out;
}

// the numbers of the first  NAME/ ... /  after a DATA keyword on a line of the Fortran (second method: comma-separated tokens between the slashes)
std::vector<double> data_numbers(const std::vector<std::string>& lines, const std::string& name) {
    for (const std::string& l : lines) {
        const std::size_t at = l.find(name + "/");
        if (l.rfind("      DATA ", 0) != 0 || at == std::string::npos) continue;
        const std::size_t start = at + name.size() + 1;
        const std::size_t end = l.find('/', start);
        REQUIRE(end != std::string::npos);
        std::vector<double> out;
        std::string body = l.substr(start, end - start);
        std::size_t pos = 0;
        while (pos <= body.size()) {
            const std::size_t comma = body.find(',', pos);
            std::string tok = squeeze(body.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
            if (!tok.empty()) out.push_back(std::strtod(tok.c_str(), nullptr));
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        return out;
    }
    FAIL("no DATA " << name);
    return {};
}

std::string between(const std::string& s, const std::string& open, const std::string& close, std::size_t from = 0) {
    const std::size_t a = s.find(open, from);
    REQUIRE(a != std::string::npos);
    const std::size_t b = s.find(close, a + open.size());
    REQUIRE(b != std::string::npos);
    return s.substr(a + open.size(), b - a - open.size());
}

}  // namespace

TEST_CASE("the pinned Fortran: the model ends at line 2,437 and the distribution's driver follows; the driver's DATA statements and calls are the ones the tool replicates", "[msis_reference][real_tree]") {
    const std::string text = universal_newlines(read_text_lossy(real_fortran()));
    const std::vector<std::string> lines = lines_of(text);
    REQUIRE(lines.size() == 2552);
    // the model's last statement, two blank lines, and then the comment that opens the driver
    CHECK(trim_right(lines.at(2434)) == "      END");
    CHECK(trim_right(lines.at(2435)).empty());
    CHECK(trim_right(lines.at(2436)).empty());
    CHECK(lines.at(2437).rfind("C      TEST DRIVER FOR GTD7 (ATMOSPHERIC MODEL)", 0) == 0);
    const std::string model = ms::model_source(text);
    const std::vector<std::string> model_lines = lines_of(model);
    CHECK(model_lines.size() == 2437);
    CHECK(model.size() == [&] {
        std::size_t n = 0;
        for (std::size_t i = 0; i < 2437; ++i) n += lines.at(i).size() + 1;
        return n;
    }());
    CHECK(model_lines.front() == lines.front());
    CHECK(model_lines.back() == lines.at(2436));
    // the DATA statements of the pinned driver are the tool's, character for character (trailing blanks aside); the tool's  DATA APH/.../,SW/.../  adds only  ,SWD/25*1./
    const std::vector<std::string> mine = lines_of(ms::driver_source());
    std::vector<std::string> mine_data;
    for (const std::string& l : mine) {
        if (l.rfind("      DATA ", 0) == 0) mine_data.push_back(l);
    }
    std::vector<std::string> pinned_data;
    for (std::size_t i = 2437; i < lines.size(); ++i) {
        if (lines.at(i).rfind("      DATA ", 0) == 0) pinned_data.push_back(trim_right(lines.at(i)));
        if (lines.at(i).find("DO I=1,15") != std::string::npos) break;
    }
    REQUIRE(mine_data.size() == 10);
    REQUIRE(pinned_data.size() == 10);
    for (std::size_t i = 0; i < 9; ++i) {
        INFO("DATA statement " << i);
        CHECK(mine_data.at(i) == pinned_data.at(i));
    }
    CHECK(mine_data.at(9) == pinned_data.at(9) + ",SWD/25*1./");
    // the three calls of GTD7 with a different argument list in the 17 published cases: the tool's equal the pinned driver's up to the output arguments
    const auto gtd7_calls = [](const std::vector<std::string>& ls, std::size_t from) {
        std::vector<std::string> out;
        for (std::size_t i = from; i + 1 < ls.size(); ++i) {
            if (ls.at(i).find("CALL GTD7(") != std::string::npos && ls.at(i).find("GTD7D") == std::string::npos) {
                std::string call = squeeze(ls.at(i)) + squeeze(ls.at(i + 1).substr(6));   // the continuation line without its first six columns (the '&' stands in the sixth)
                const std::size_t cut = call.find(",48,");
                REQUIRE(cut != std::string::npos);
                out.push_back(call.substr(0, cut + 4));
                if (out.size() == 3) break;
            }
        }
        return out;
    };
    const std::vector<std::string> pinned_calls = gtd7_calls(lines, 2437);
    const std::vector<std::string> my_calls = gtd7_calls(mine, 0);
    REQUIRE(pinned_calls.size() == 3);
    CHECK(my_calls == pinned_calls);
    CHECK(pinned_calls.at(0) == "CALLGTD7(IDAY(I),UT(I),ALT(I),XLAT(I),XLONG(I),XLST(I),F107A(I),F107(I),AP(I),48,");
    CHECK(pinned_calls.at(2) == "CALLGTD7(IDAY(1),UT(1),ALT(4),XLAT(1),XLONG(1),XLST(1),F107A(1),F107(1),APH,48,");
}

TEST_CASE("the sweep crosses every boundary the pinned Fortran declares", "[msis_reference][real_tree]") {
    const std::vector<std::string> lines = lines_of(universal_newlines(read_text_lossy(real_fortran())));
    const std::vector<double>& alt = ms::sweep_alt();
    const auto in_sweep = [&](double a) { return std::find(alt.begin(), alt.end(), a) != alt.end(); };
    // the nodes of the lower atmosphere: ZN3 (32.5 20 15 10 0), ZN2 (72.5 55 45 32.5), ZN1 (120 110 100 90 72.5) and the mixing height ZMIX (62.5): each is an altitude of the sweep
    for (const char* name : {"ZN3", "ZN2", "ZN1", "ZMIX"}) {
        const std::vector<double> nodes = data_numbers(lines, name);
        REQUIRE_FALSE(nodes.empty());
        for (const double a : nodes) {
            INFO(name << " " << a);
            CHECK(in_sweep(a));
        }
    }
    CHECK(data_numbers(lines, "ZN3") == std::vector<double>{32.5, 20.0, 15.0, 10.0, 0.0});
    CHECK(data_numbers(lines, "ZN2") == std::vector<double>{72.5, 55.0, 45.0, 32.5});
    CHECK(data_numbers(lines, "ZN1") == std::vector<double>{120.0, 110.0, 100.0, 90.0, 72.5});
    CHECK(data_numbers(lines, "ZMIX") == std::vector<double>{62.5});
    // the species-correction cutoffs (DATA ALTL): the sweep has a point 10 m below and 10 m above each
    const std::vector<double> altl = data_numbers(lines, "ALTL");
    CHECK(altl == std::vector<double>{200.0, 300.0, 160.0, 250.0, 240.0, 450.0, 320.0, 450.0});
    for (const double c : altl) {
        INFO("cutoff " << c);
        CHECK(in_sweep(std::round((c - 0.01) * 1e6) / 1e6));
        CHECK(in_sweep(std::round((c + 0.01) * 1e6) / 1e6));
    }
    // and the sweep's conditions include the driver's own baseline (day 172, 29000 s, 60 N, 70 W, F10.7 150/150, Ap 4)
    CHECK(ms::sweep_cond().front().iday == 172);
    CHECK(ms::sweep_cond().front().ap == 4.0);
}

TEST_CASE("the committed msis_reference_values.hpp, rebuilt by emit() from the data it holds, is itself", "[msis_reference][real_tree]") {
    const std::string committed = read_text(tree_path("modules/atmosphere/src/msis_reference_values.hpp"));
    const std::vector<std::string> lines = lines_of(committed);
    const std::string sha = between(committed, "// implementation NRLMSISE-00.FOR, sha256 ", "\n");
    const std::string compiler = between(committed, "//   compiler : ", "\n");
    const auto count = [&](const char* label) { return number_after(committed, label); };
    const std::size_t a = count("kClassMaterial    = ");
    const std::size_t b = count("kClassImmaterial  = ");
    const std::size_t c = count("kClassUnderflowed = ");
    const std::size_t z = count("kClassZero        = ");
    const double median = std::strtod(between(committed, "kSingleMedianRelative = ", ";").c_str(), nullptr);
    const double worst = std::strtod(between(committed, "kSingleWorstRelative  = ", ";").c_str(), nullptr);
    const double immaterial_worst = std::strtod(between(committed, "//   B  immaterial   ", "\n").substr(between(committed, "//   B  immaterial   ", "\n").find("worst ") + 6).c_str(), nullptr);
    // the sensitivities and the worst comparison, from the two paragraphs
    const std::string p1 = paragraph(lines, "// The class boundary is physical");
    double sensitivity[3] = {0.0, 0.0, 0.0};
    sensitivity[0] = std::strtod(between(p1, "class A's worst is ", " at a threshold of 1e-12").c_str(), nullptr);
    sensitivity[1] = std::strtod(between(p1, "1e-12 and ", " at 1e-15,").c_str(), nullptr);
    sensitivity[2] = std::strtod(between(p1, "and only ", " at 1e-20.").c_str(), nullptr);
    const std::string p2 = paragraph(lines, "// The worst class-A comparison");
    const std::string species = between(p2, "comparison is ", " at sweep alt ");
    const std::string place = "sweep alt " + between(p2, " at sweep alt ", " -- a SWEEP point");
    const double worst_published = std::strtod(between(p2, "(whose own worst is ", ").").c_str(), nullptr);
    const std::size_t sweep_material = static_cast<std::size_t>(std::strtoull(between(p2, "it LOOSENS IT: the ", " further").c_str(), nullptr, 10));
    // the key of the worst comparison, found from its place by the test's own table
    long long worst_index = 0;
    for (long long i = 1; i <= 164; ++i) {
        if (place_of("S", i) == place) worst_index = i;
    }
    REQUIRE(worst_index != 0);
    const auto q = std::find(kSpecies.begin(), kSpecies.end(), species);
    REQUIRE(q != kSpecies.end());
    const std::size_t worst_quantity = static_cast<std::size_t>(q - kSpecies.begin());

    ms::Classes classes;
    classes.material.assign(a, ms::Comparison{median, g7("S", 1), 0});
    classes.material.at(0) = ms::Comparison{worst, g7("S", worst_index), worst_quantity};
    const std::size_t published = a - sweep_material;
    REQUIRE(published >= 1);
    REQUIRE(published < a / 2);
    for (std::size_t k = 1; k <= published; ++k) classes.material.at(k) = ms::Comparison{k == 1 ? worst_published : 1e-7, g7("P", 1), 0};
    classes.immaterial.assign(b, ms::Comparison{1e-9, g7("S", 1), 0});
    classes.immaterial.at(0) = ms::Comparison{immaterial_worst, g7("S", 1), 0};
    classes.zeros = z;
    // the first six underflows, as the header lists them, and the others (their number is the rest of class C)
    std::size_t listed = 0;
    for (const std::string& l : lines) {
        if (l.rfind("//   underflow: ", 0) != 0) continue;
        std::istringstream in(l);
        std::string slashes, word, kind, setindex, quantity_word, quantity, single_word, single_zero, double_word, value;
        in >> slashes >> word >> kind >> setindex >> quantity_word >> quantity >> single_word >> single_zero >> double_word >> value;
        REQUIRE_FALSE(value.empty());
        classes.underflowed.push_back(ms::Underflow{ms::Key{kind, setindex.substr(0, 1), std::stoll(setindex.substr(1))}, std::stoul(quantity), std::strtod(value.c_str(), nullptr)});
        ++listed;
    }
    CHECK(listed == 6);
    while (classes.underflowed.size() < c) classes.underflowed.push_back(ms::Underflow{g7("S", 1), 8, 1e-50});
    // the double-precision records, from the rows of kValues
    ms::Results dbl;
    std::size_t rows = 0;
    for (const std::string& l : lines) {
        if (l.rfind("    {'", 0) != 0) continue;
        std::string cleaned = l;
        for (char& ch : cleaned) {
            if (ch == '{' || ch == '}' || ch == ',' || ch == '\'') ch = ' ';
        }
        std::istringstream in(cleaned);
        std::string set;
        long long index = 0;
        in >> set >> index;
        std::vector<double> numbers;
        std::string tok;
        while (in >> tok) numbers.push_back(std::strtod(tok.c_str(), nullptr));
        REQUIRE(numbers.size() == 12);
        dbl.set(g7(set.c_str(), index), std::vector<double>(numbers.begin(), numbers.begin() + 11));
        dbl.set(g7d(set.c_str(), index), {numbers.at(11)});
        ++rows;
    }
    CHECK(rows == 181);
    // emit() rebuilds the file: the header and the table, both paragraphs wrapped at 77 columns, the underflow lines, the constants and 181 rows of 17 digits
    CHECK(ms::emit(compiler, sha, classes, dbl, sensitivity) == committed);
}

TEST_CASE("the committed header's counts agree with one another and with the sweep", "[msis_reference][real_tree]") {
    const std::string committed = read_text(tree_path("modules/atmosphere/src/msis_reference_values.hpp"));
    const std::size_t a = number_after(committed, "kClassMaterial    = ");
    const std::size_t b = number_after(committed, "kClassImmaterial  = ");
    const std::size_t c = number_after(committed, "kClassUnderflowed = ");
    const std::size_t z = number_after(committed, "kClassZero        = ");
    CHECK(number_after(committed, "kPublishedCases = ") == 17);
    CHECK(number_after(committed, "kSweepCases     = ") == ms::sweep_alt().size() * ms::sweep_cond().size());
    CHECK(a + b + c + z == 2172);   // 181 records x 12 quantities
    CHECK(2172 == (17 + ms::sweep_alt().size() * ms::sweep_cond().size()) * 12);
    CHECK(contains(committed, "//   total           " + pad5(2172) + "   = 181 records x 12 quantities\n"));
    CHECK(contains(committed, "inline constexpr std::array<Record, 181> kValues{{\n"));
    CHECK(contains(committed, "// Produced by tools/msis_reference.cpp from the hash-pinned NRL reference\n"));
}

TEST_CASE("gfortran on the pinned Fortran reproduces the committed header (regeneration only: needs the compiler the header records, run when asked for by tag)", "[.regeneration][msis_reference]") {
    // The committed header was made on 2026-09-23 with the compiler its recipe line names; the first comparison of group C10 reproduced it byte for byte with the left-over executables of that run and, with the
    // installed gfortran-16, from the sources.  A different compiler changes the recipe line (and may change the numbers): compare the value rows first before concluding anything about the tool.
    const TempDir td("odl-msisref");
    ms::Settings s = ms::default_settings();
    s.work = td.path();
    const Result r = run_with(s, {"--check"});
    if (r.code == 0 && contains(r.out, "no Fortran compiler")) SKIP("no Fortran compiler on PATH");
    INFO(r.out << r.err);
    CHECK(r.code == 0);
}
