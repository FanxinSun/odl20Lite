// msis_reference.cpp — freeze the NRLMSISE-00 reference implementation's output.
//
// Plan L0 step 8, group C10, ported to C++ (the user's directive of 2026-10-06) from tools/msis_reference.py (deleted by the same commit: `git show fb4b960:rewrite/tools/msis_reference.py`).
//
// SPEC-atmosphere ATMO-R-028.  The gate must run offline from the cache and with no Fortran compiler, so the reference's output is committed as generated source and `ATMO-A-001` compares against that.
// Where a compiler IS present this tool regenerates the file and the suite diffs it, so the frozen artefact is itself under test rather than trusted.
//
// WHY THIS TOOL EXISTS AT ALL, which is the whole of step 4's finding: NRL publishes NO reference value for NRLMSISE-00.  The distribution's test driver publishes 17 fully specified INPUT cases and no
// expected output; the archive tables publish output statistics against a database NRL does not ship.  The reference implementation is therefore normative rather than an oracle — plan §4 rule 6 — and its
// output on its own published inputs is a category-1 acceptance value.  What that cannot check is stated in SPEC-atmosphere §8.
//
// THE PUBLISHED DRIVER'S OWN OUTPUT IS NOT ENOUGH.  It prints `1P8E9.2` — three significant figures — and its summary table four, against a model whose own value is defined to about six (§3.2).  So this
// tool replicates the driver's `DATA` statements EXACTLY and changes only the output FORMAT.  The reference source is NOT edited.  `-fdec-char-conversions` exists because three DATA statements at lines
// 1670-1671 use the FORTRAN 66 Hollerith idiom for the model's output header stamp, which touches no arithmetic.
//
// THE USER'S DECISION OF 2026-10-06, recorded here as the plan asks: "The Fortran oracle stays."  NRL's published NRLMSISE-00 Fortran, unchanged, remains the atmosphere model's independent reference,
// compiled by the installed `gfortran-16` WHEN THE REFERENCES ARE REGENERATED; its driver (the 65 lines of driver_source(), the Python's string) and its wrapper (this file) are C++.  So `model.f` stays
// Fortran, as published, and this tool never changes it.
//
// HOST PROGRAM.  The tool spawns `gfortran` (the first of gfortran, gfortran-16, -15, -14, -13 on PATH).  It is a regeneration-only host program: it is NOT part of the build, NOT run by CMake, ctest or
// ci.sh, NOT declared in the manifest and NOT redistributed (the maintainer's ruling D9); with no compiler on PATH the tool says so and does nothing, exit 0, as the Python did.  Nothing in the build or CI
// may come to need gfortran.  The tool's own tests use a stand-in compiler.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C10_proof_registration.txt in the group's report files).  The tool's product is one committed text,
// modules/atmosphere/src/msis_reference_values.hpp (63,104 bytes, 279 lines), naming the generator on line 4; the registered comparison is byte for byte but for that name, with controls on the Fortran
// side that are independent of every line of this file (the left-over model.f, driver.f and executables of the Python's run of 2026-09-23).  C10_proof_result.txt holds the result as it came.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * What the Python ended with sys.exit(message) ends with the same message and exit 1 (Fatal); what was a traceback there (a manifest field missing, a line of the reference's output that does not
//     parse, a missing key) is a refusal naming the fault, exit 2 (BadInput).  An error the tool did not anticipate exits 70.
//   * The options are taken by their exact names (argparse took any unambiguous abbreviation); `-h` prints this tool's own text.  New: `--root DIR` (the tree to read; default: the tree this tool was built
//     from), `--work DIR` (where the Fortran is built and run; default: tools/.msisref, git-ignored) and `--sweep-input FILE` (write the standard input of the sweep there, which the controls of the proof
//     feed to the Python-era executables).
//   * The compiler is run by the path `which` found, where the Python passed the bare name; the message names the bare name.

#include "msis_reference.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/process.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>

#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>

namespace dk = odl::devkit;
using odl::devkit::Streams;

namespace odl::tools::msis_reference {

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;

constexpr double kAmu = 1.66e-24;
constexpr char kPromote[] = "-freal-4-real-8";   // same source, every REAL widened: isolates precision artefacts
const std::vector<std::string>& flags() {
    static const std::vector<std::string> f = {"-std=legacy", "-fdec-char-conversions", "-O0"};
    return f;
}
std::string flags_text() {
    std::string out;
    for (const std::string& f : flags()) out += (out.empty() ? "" : " ") + f;
    return out;
}

// The sweep.  Chosen to cross EVERY branch boundary the model has (SPEC-atmosphere §3.6): ZN3 nodes 0/10/15/20/32.5, ZN2 45/55/72.5, ZMIX 62.5, ZN1 90/100/110/120, and the analytic Bates profile above.
// A sweep that never crosses a boundary cannot fail on a boundary.
//
// ABOVE 120 KM, "no structural boundary" (SPEC-atmosphere §3.1's own former text) was itself a claim that had never been searched for in the source -- L4 step 4's own drag Jacobian found the 300 km one by
// measurement, and a direct read of the pinned NRLMSISE-00.FOR's DATA ALTL (line 587) found the other six: N2 160, He 200, Ar 240, O2 250, O 300, H 320, N 450 km, each a species-correction cutoff and each a
// genuine discontinuity (confirmed: the reference jumps identically to the port at every one, to full double precision -- PROVENANCE.md §28.5/§28.10). 200 and 300 already sat in the coarse grid above without
// being fine enough to straddle either cutoff by more than a few metres; the straddle adds a close pair either side of all seven, so the sweep that is supposed to cross every boundary the model has actually
// crosses these, rather than landing near them and calling that coverage.
constexpr double kCutoffStraddleKm[] = {160.0, 200.0, 240.0, 250.0, 300.0, 320.0, 450.0};

// round(x, 6): Python's float.__round__ rounds the exact binary value to six decimals (round-half-even) and reads the nearest double back — printf's %.6f and strtod.
double round6(double x) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6f", x);
    return std::strtod(buf, nullptr);
}

const std::map<std::size_t, double>& species_mass() {
    static const std::map<std::size_t, double> m = {{0, 4.0}, {1, 16.0}, {2, 28.0}, {3, 32.0}, {4, 40.0}, {6, 1.0}, {7, 14.0}, {8, 16.0}};
    return m;
}
const char* species_name(std::size_t j) {
    static const std::array<const char*, 11> names = {"He", "O", "N2", "O2", "Ar", "rho", "H", "N", "anomalous O", "Tinf", "T(alt)"};
    return j < names.size() ? names.at(j) : nullptr;
}

std::vector<std::string> split_at_newline(const std::string& s) {
    std::vector<std::string> out;
    std::size_t from = 0;
    for (;;) {
        const std::size_t at = s.find('\n', from);
        if (at == std::string::npos) {
            out.push_back(s.substr(from));
            return out;
        }
        out.push_back(s.substr(from, at - from));
        from = at + 1;
    }
}

bool parse_int(std::string_view token, long long& out) {
    if (token.empty()) return false;
    const char* first = token.data();
    const char* last = token.data() + token.size();
    if (*first == '+') ++first;   // from_chars does not take a plus sign; int() does
    const auto r = std::from_chars(first, last, out);
    return r.ec == std::errc() && r.ptr == last;
}

bool parse_float(const std::string& token, double& out) {
    if (token.empty()) return false;
    char* end = nullptr;
    out = std::strtod(token.c_str(), &end);
    return end == token.c_str() + token.size();
}

// the last `n` characters of `s` (a Python slice s[-n:] of the decoded text: n code points, however many bytes they take; fewer than n, or n = 0, is the whole text).  The bytes are the compiler's or the reference's
// standard error; a byte that starts a code point is one that is not a continuation byte (10xxxxxx), and a stray continuation byte counts for nothing.
std::string tail_of(const std::string& s, std::size_t n) {
    std::size_t starts = 0;
    for (std::size_t from = s.size(); from > 0;) {
        --from;
        if ((static_cast<unsigned char>(s[from]) & 0xC0) != 0x80 && ++starts == n) return s.substr(from);
    }
    return s;
}

}  // namespace

const std::vector<double>* Results::find(const Key& key) const {
    for (const auto& item : items) {
        if (item.first == key) return &item.second;
    }
    return nullptr;
}

void Results::set(const Key& key, std::vector<double> values) {
    for (auto& item : items) {
        if (item.first == key) {
            item.second = std::move(values);
            return;
        }
    }
    items.emplace_back(key, std::move(values));
}

const std::vector<double>& sweep_alt() {
    static const std::vector<double> alt = [] {
        std::vector<double> v = {0.0,  5.0,  10.0, 15.0,  20.0,  25.0,  32.5,   40.0,   45.0,   55.0,   62.5,   70.0,   72.5,   80.0,
                                 90.0, 100.0, 110.0, 120.0, 150.0, 200.0, 300.0, 400.0, 550.0, 700.0, 1000.0, 1500.0, 2000.0};
        for (const double c : kCutoffStraddleKm) {
            for (const double offset_m : {-10.0, 10.0}) v.push_back(round6(c + offset_m / 1000.0));
        }
        return v;
    }();
    return alt;
}

const std::vector<Condition>& sweep_cond() {
    static const std::vector<Condition> cond = {
        // iday,  sec,   lat,   lon, f107a, f107,  ap
        {172, 29000.0, 60.0, -70.0, 150.0, 150.0, 4.0},     // the driver's baseline
        {81, 29000.0, 0.0, 0.0, 70.0, 70.0, 0.0},           // equinox, equator, solar minimum, quiet
        {355, 75000.0, -80.0, 170.0, 250.0, 300.0, 200.0},  // southern winter, solar max, severe storm
        {200, 43200.0, 45.0, 90.0, 100.0, 90.0, 15.0},      // mid-latitude, moderate
    };
    return cond;
}

std::string driver_source() {
    // The 17 published cases verbatim, then a sweep read from stdin.  (The Python's textwrap.dedent of an indented string; this is the dedented text.)
    return
        "C     GENERATED by tools/msis_reference.cpp -- do not edit.\n"
    "C     Part 1 replicates NRLMSISE-00.FOR lines 2438-2552's DATA statements\n"
    "C     EXACTLY; only the output FORMAT differs.  Part 2 reads a sweep.\n"
    "      DIMENSION D(9),T(2),SW(25),SWD(25),APH(7)\n"
    "      DIMENSION IDAY(15),UT(15),ALT(15),XLAT(15),XLONG(15),XLST(15),\n"
    "     & F107A(15),F107(15),AP(15)\n"
    "      DATA IDAY/172,81,13*172/\n"
    "      DATA UT/29000.,29000.,75000.,12*29000./\n"
    "      DATA ALT/400.,400.,1000.,100.,6*400.,0,10.,30.,50.,70./\n"
    "      DATA XLAT/4*60.,0.,10*60./\n"
    "      DATA XLONG/5*-70.,0.,9*-70./\n"
    "      DATA XLST/6*16.,4.,8*16./\n"
    "      DATA F107A/7*150.,70.,7*150./\n"
    "      DATA F107/8*150.,180.,6*150./\n"
    "      DATA AP/9*4.,40.,5*4./\n"
    "      DATA APH/7*100./,SW/8*1.,-1.,16*1./,SWD/25*1./\n"
    "      DO I=1,15\n"
    "         CALL GTD7(IDAY(I),UT(I),ALT(I),XLAT(I),XLONG(I),XLST(I),\n"
    "     &             F107A(I),F107(I),AP(I),48,D,T)\n"
    "         CALL WROUT('P',I,D,T)\n"
    "         CALL GTD7D(IDAY(I),UT(I),ALT(I),XLAT(I),XLONG(I),XLST(I),\n"
    "     &             F107A(I),F107(I),AP(I),48,D,T)\n"
    "         CALL WRRHO('P',I,D(6))\n"
    "      ENDDO\n"
    "      CALL TSELEC(SW)\n"
    "      CALL GTD7(IDAY(1),UT(1),ALT(1),XLAT(1),XLONG(1),XLST(1),\n"
    "     &             F107A(1),F107(1),APH,48,D,T)\n"
    "      CALL WROUT('P',16,D,T)\n"
    "      CALL GTD7D(IDAY(1),UT(1),ALT(1),XLAT(1),XLONG(1),XLST(1),\n"
    "     &             F107A(1),F107(1),APH,48,D,T)\n"
    "      CALL WRRHO('P',16,D(6))\n"
    "      CALL GTD7(IDAY(1),UT(1),ALT(4),XLAT(1),XLONG(1),XLST(1),\n"
    "     &             F107A(1),F107(1),APH,48,D,T)\n"
    "      CALL WROUT('P',17,D,T)\n"
    "      CALL GTD7D(IDAY(1),UT(1),ALT(4),XLAT(1),XLONG(1),XLST(1),\n"
    "     &             F107A(1),F107(1),APH,48,D,T)\n"
    "      CALL WRRHO('P',17,D(6))\n"
    "C     ---- part 2: the sweep, one case per line on stdin ----\n"
    "      CALL TSELEC(SWD)\n"
    "      READ(5,*) NS\n"
    "      DO K=1,NS\n"
    "         READ(5,*) JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD\n"
    "         CALL GTD7(JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD,48,D,T)\n"
    "         CALL WROUT('S',K,D,T)\n"
    "         CALL GTD7D(JD,SEC,ZALT,GLAT,GLON,STL,FA,FD,APD,48,D,T)\n"
    "         CALL WRRHO('S',K,D(6))\n"
    "      ENDDO\n"
    "      END\n"
    "      FUNCTION SW2()\n"
    "      DIMENSION SW2(25)\n"
    "      DO J=1,25\n"
    "         SW2(J)=1.\n"
    "      ENDDO\n"
    "      END\n"
    "      SUBROUTINE WROUT(TAG,I,D,T)\n"
    "      CHARACTER*1 TAG\n"
    "      DIMENSION D(9),T(2)\n"
    "      WRITE(6,100) TAG,I,(D(J),J=1,9),T(1),T(2)\n"
    "  100 FORMAT('G7 ',A1,I5,11(1X,1PE25.17))\n"
    "      END\n"
    "      SUBROUTINE WRRHO(TAG,I,R)\n"
    "      CHARACTER*1 TAG\n"
    "      WRITE(6,101) TAG,I,R\n"
    "  101 FORMAT('G7D ',A1,I5,1X,1PE25.17)\n"
    "      END\n"
        ;
}

std::string model_source(std::string_view fortran_text) {
    // "\n".join(text.split("\n")[:2437]) + "\n"
    std::size_t at = 0;
    std::size_t seen = 0;
    while (seen < kDriverSplit) {
        const std::size_t nl = fortran_text.find('\n', at);
        if (nl == std::string_view::npos) return std::string(fortran_text) + "\n";   // fewer lines than that: the whole text, one more newline
        at = nl + 1;
        ++seen;
    }
    return std::string(fortran_text.substr(0, at));
}

std::string sweep_input() {
    std::string text = std::to_string(sweep_alt().size() * sweep_cond().size()) + "\n";
    for (const Condition& c : sweep_cond()) {
        for (const double alt : sweep_alt()) {
            const double stl = c.sec / 3600.0 + c.lon / 15.0;
            text += std::to_string(c.iday) + " " + dk::py_float_repr(c.sec) + " " + dk::py_float_repr(alt) + " " + dk::py_float_repr(c.lat) + " " + dk::py_float_repr(c.lon) + " " +
                    dk::py_float_repr(stl) + " " + dk::py_float_repr(c.f107a) + " " + dk::py_float_repr(c.f107) + " " + dk::py_float_repr(c.ap) + "\n";
        }
    }
    return text;
}

Results parse_output(std::string_view text) {
    Results out;
    for (const std::string& line : split_at_newline(dk::universal_newlines(text))) {
        const std::vector<std::string> p = dk::split_py(line);
        if (p.empty()) continue;
        if (p[0] != "G7" && p[0] != "G7D") continue;
        const bool g7 = p[0] == "G7";
        const std::size_t first_value = 3;
        if (p.size() < (g7 ? first_value : first_value + 1)) throw BadInput("a line of the reference's output is too short: " + line);
        Key key;
        key.kind = g7 ? "g7" : "g7d";
        key.set = p[1];
        if (!parse_int(p[2], key.index)) throw BadInput("the index in a line of the reference's output is not an integer: " + line);
        std::vector<double> values;
        const std::size_t last = g7 ? p.size() : first_value + 1;   // G7D takes the first number only: float(p[3])
        for (std::size_t k = first_value; k < last; ++k) {
            double v = 0.0;
            if (!parse_float(p[k], v)) throw BadInput("a number in a line of the reference's output does not parse: " + p[k]);
            values.push_back(v);
        }
        out.set(key, std::move(values));
    }
    return out;
}

namespace {

// (r, key, quantity) sorted in reverse, as Python's  list.sort(reverse=True)  of tuples: the larger relative difference first, ties by key then quantity, descending
bool comparison_before(const Comparison& a, const Comparison& b) {
    if (a.relative != b.relative) return a.relative > b.relative;
    if (a.key != b.key) return a.key > b.key;
    return a.quantity > b.quantity;
}

}  // namespace

Classes classify(const Results& single, const Results& dbl, double material) {
    // THE SWEEP CHANGED THE SHAPE OF THIS MEASUREMENT.  Over the 17 published cases the reference's single-precision artefact looks like one number: median 4e-7, worst 8e-6.  Over a sweep that crosses every
    // branch boundary it does not -- the worst relative difference is 7.9e-3, a thousand times larger.
    //
    // Every one of those large differences is a quantity of order 1e-30 to 1e-37: anomalous oxygen at 110 km, argon at 2000 km, hydrogen at 72.5 km.  Single precision's smallest normal is 1.18e-38, so these
    // are values approaching the underflow boundary, losing significance continuously on the way down until they cross it and become the hard zeros of the underflow class.  It is ONE phenomenon in three
    // regimes, not a tolerance with an exception bolted on.
    //
    // So the boundary is drawn PHYSICALLY rather than numerically: a species whose mass contributes less than `material` of the total density cannot affect drag at any precision.  That threshold is not tuned
    // -- the worst material difference moves by less than a factor of 2 across three decades of it (1e-12, 1e-15, 1e-20; main() recomputes and emit() prints the current figures each time).  Stable across
    // three decades is what distinguishes a principled boundary from a fitted one.
    Classes c;
    for (const auto& [k, dv] : dbl.items) {
        const std::vector<double>* sv = single.find(k);
        if (sv == nullptr) throw BadInput("the single-precision run has no record " + k.kind + " " + k.set + std::to_string(k.index));
        const bool is_g7 = k.kind == "g7";
        if (dv.size() <= (is_g7 ? 5u : 0u)) throw BadInput("the record " + k.kind + " " + k.set + std::to_string(k.index) + " is too short");
        const std::size_t n = std::min(sv->size(), dv.size());
        for (std::size_t j = 0; j < n; ++j) {
            const double xs = (*sv)[j];
            const double xd = dv[j];
            if (xd == 0.0) {
                ++c.zeros;
                continue;
            }
            if (xs == 0.0) {
                c.underflowed.push_back({k, j, xd});
                continue;
            }
            const double r = std::fabs(xs - xd) / std::fabs(xd);
            const auto mass = species_mass().find(j);
            const double frac = (is_g7 && mass != species_mass().end()) ? mass->second * xd * kAmu / dv[5] : 1.0;   // dv[5], the total density, is there: checked above
            (frac >= material ? c.material : c.immaterial).push_back({r, k, j});
        }
    }
    std::sort(c.material.begin(), c.material.end(), comparison_before);
    std::sort(c.immaterial.begin(), c.immaterial.end(), comparison_before);
    return c;
}

std::string describe(const Key& key) {
    // A worst-comparison key as a human-readable place, computed from the SAME loop order sweep_input() uses to build it -- not hardcoded, so this description cannot go stale the way a literal string did
    // the first time the sweep grew (msis_reference_values.hpp's own history, PROVENANCE.md §28.5/§28.10).
    if (key.set == "P") return "published case " + std::to_string(key.index);
    const std::size_t n = static_cast<std::size_t>(key.index - 1) % sweep_alt().size();
    const std::size_t c = static_cast<std::size_t>(key.index - 1) / sweep_alt().size();
    const Condition& cond = sweep_cond().at(c);
    return "sweep alt " + dk::py_format_g(sweep_alt().at(n)) + " km, condition (" + std::to_string(cond.iday) + "," + dk::py_format_g(cond.sec) + "," + dk::py_format_g(cond.lat) + "," +
           dk::py_format_g(cond.lon) + "," + dk::py_format_g(cond.f107a) + "," + dk::py_format_g(cond.f107) + "," + dk::py_format_g(cond.ap) + ")";
}

namespace {

std::string pad5(std::size_t n) { return dk::pad_left(std::to_string(n), 5); }

}  // namespace

std::string emit(const std::string& compiler, const std::string& sha256_of_source, const Classes& classes, const Results& dbl, const double (&sensitivity)[3]) {
    const std::vector<Comparison>& mat = classes.material;
    const std::vector<Comparison>& immat = classes.immaterial;
    const std::vector<Underflow>& under = classes.underflowed;
    const std::size_t zeros = classes.zeros;
    if (mat.empty() || immat.empty()) throw BadInput("the classification has no material or no immaterial comparison: the header would have no worst case to state");
    const double med = mat[mat.size() / 2].relative;

    std::vector<std::string> L;
    const auto w = [&L](std::string line) { L.push_back(std::move(line)); };
    const auto wc = [&L](const std::string& text) {
        dk::WrapOptions options;
        options.width = 77;
        for (const std::string& line : dk::wrap_py(text, options)) L.push_back("// " + line);
    };
    w("#pragma once");
    w("// msis_reference_values.hpp — GENERATED.  Do not edit.");
    w("//");
    w("// Produced by tools/msis_reference.cpp from the hash-pinned NRL reference");
    w("// implementation NRLMSISE-00.FOR, sha256 " + sha256_of_source);
    w("//");
    w("// SPEC-atmosphere ATMO-R-028.  NRL PUBLISHES NO REFERENCE VALUE FOR THIS MODEL:");
    w("// its test driver publishes 17 fully specified INPUT cases and no expected");
    w("// output, and its archive tables are statistics against a database NRL does not");
    w("// ship.  The reference implementation is therefore NORMATIVE rather than an");
    w("// oracle (plan §4 rule 6), and these are its own values on its own published");
    w("// inputs.  What that cannot check is stated in SPEC-atmosphere §8: nothing here");
    w("// can tell whether the FORTRAN computes what the paper describes.");
    w("//");
    w("// THE RECIPE, so that this file is reproducible rather than merely present:");
    w("//   compiler : " + compiler);
    w("//   flags    : " + flags_text());
    w("//   promoted : " + flags_text() + " " + kPromote + "   (same source, every REAL widened)");
    w("//   source   : lines 1..2437 of the pinned file; the distribution's own driver");
    w("//              at 2438..2552 is replaced by one whose DATA statements are");
    w("//              IDENTICAL and whose output FORMAT is not -- the published driver");
    w("//              prints three significant figures against a model defined to six.");
    w("//   unedited : the reference source is not modified.  -fdec-char-conversions");
    w("//              accepts three FORTRAN 66 Hollerith DATA statements at lines");
    w("//              1670-1671 carrying the output header stamp and no arithmetic.");
    w("//   -O0      : deliberate.  Optimisation may reassociate single-precision");
    w("//              arithmetic and move the reference's own value by more than the");
    w("//              median artefact measured below.");
    w("//");
    const std::size_t tot = mat.size() + immat.size() + under.size() + zeros;
    w("// THE TOLERANCE IS DERIVED HERE, not asserted elsewhere.  Single precision");
    w("// against the promoted-double build of the SAME source, every comparison");
    w("// classified by whether it can affect a drag calculation at all:");
    w("//");
    w("//   A  material     " + pad5(mat.size()) + "   median " + dk::py_format_e(med, 4) + "   worst " + dk::py_format_e(mat[0].relative, 4));
    w("//   B  immaterial   " + pad5(immat.size()) + "                        worst " + dk::py_format_e(immat[0].relative, 4));
    w("//   C  underflowed  " + pad5(under.size()) + "   single returned exactly zero, double did not");
    w("//   Z  zero in both " + pad5(zeros) + "   the documented zeros below 72.5 km, and anomalous O");
    w("//   total           " + pad5(tot) + "   = " + std::to_string(dbl.items.size() / 2) + " records x 12 quantities");
    w("//");
    w("// The reference is SINGLE PRECISION throughout (no DOUBLE PRECISION, no REAL*8,");
    w("// no .D0 anywhere in it), so its own value is uncertain and NO TOLERANCE MAY BE");
    w("// TIGHTER than class A's worst.  Classes B and C are not a tolerance question:");
    w("// they are quantities of order 1e-30 and below, approaching and then crossing");
    w("// single precision's smallest normal (1.18e-38), where a correct double port");
    w("// disagrees with the reference by up to 100% and is right to.  They are COUNTED,");
    w("// never absorbed into a widened tolerance -- the third time this tree has met a");
    w("// quantity that exists in one precision and not another.");
    w("//");
    wc("The class boundary is physical, not numerical: a species whose mass is less than " + dk::py_format_g(kMaterial) +
       " of the total density cannot affect drag.  It is not tuned -- class A's worst is " + dk::py_format_e(sensitivity[0], 3) + " at a threshold of 1e-12 and " +
       dk::py_format_e(sensitivity[1], 3) + " at 1e-15, and only " + dk::py_format_e(sensitivity[2], 3) + " at 1e-20.");
    w("//");
    const char* known = species_name(mat[0].quantity);
    const std::string worst_species = known != nullptr ? known : "quantity " + std::to_string(mat[0].quantity);
    std::size_t p_material = 0;
    double worst_among_published = 0.0;
    for (const Comparison& m : mat) {
        if (m.key.set == "P") {
            ++p_material;
            worst_among_published = std::max(worst_among_published, m.relative);
        }
    }
    const std::size_t s_material = mat.size() - p_material;
    if (mat[0].key.set == "P") {
        wc("The worst class-A comparison is " + worst_species + " at " + describe(mat[0].key) + " -- one of the 17 published cases: the " + std::to_string(s_material) +
           " further material comparisons the sweep adds, across every branch boundary, do NOT exceed it.");
    } else {
        wc("The worst class-A comparison is " + worst_species + " at " + describe(mat[0].key) + " -- a SWEEP point, not one of the 17 published cases (whose own worst is " +
           dk::py_format_e(worst_among_published, 3) + "). THE SWEEP DOES NOT MERELY FAIL TO LOOSEN THE BOUND HERE, it LOOSENS IT: the " + std::to_string(s_material) +
           " further material comparisons it adds found a worse one than any of the 17 published cases did.");
    }
    for (std::size_t i = 0; i < under.size() && i < 6; ++i) {
        const Underflow& u = under[i];
        w("//   underflow: " + u.key.kind + " " + u.key.set + std::to_string(u.key.index) + " quantity " + std::to_string(u.quantity) + ": single 0, double " + dk::py_format_e(u.value, 6));
    }
    if (under.size() > 6) w("//   ... and " + std::to_string(under.size() - 6) + " more, all anomalous oxygen below 120 km");
    w("");
    w("#include <array>");
    w("#include <cstddef>");
    w("");
    w("namespace odl::atmosphere::reference {");
    w("");
    w("/// One evaluation: the nine densities, then exospheric and local temperature.");
    w("struct Record {");
    w("    char    set;        ///< 'P' a published driver case, 'S' a sweep point");
    w("    int     index;      ///< 1-based within its set");
    w("    double  gtd7[11];   ///< He O N2 O2 Ar rho H N anomO Tinf T(alt)");
    w("    double  gtd7d_rho;  ///< GTD7D's total mass density, which is NOT gtd7[5]");
    w("};");
    w("");
    w("/// The measured precision of the reference itself; the derivation is above.");
    w("/// A gate asserts these counts, not a verdict: a pass/fail over 1500");
    w("/// comparisons with one tolerance either fails on values a correct port is");
    w("/// right to produce, or passes with a tolerance that stops checking.");
    w("inline constexpr double kMaterialFraction     = " + dk::py_format_e(kMaterial, 0) + ";");
    w("inline constexpr double kSingleMedianRelative = " + dk::py_format_e(med, 6) + ";");
    w("inline constexpr double kSingleWorstRelative  = " + dk::py_format_e(mat[0].relative, 6) + ";");
    w("inline constexpr std::size_t kClassMaterial    = " + std::to_string(mat.size()) + ";");
    w("inline constexpr std::size_t kClassImmaterial  = " + std::to_string(immat.size()) + ";");
    w("inline constexpr std::size_t kClassUnderflowed = " + std::to_string(under.size()) + ";");
    w("inline constexpr std::size_t kClassZero        = " + std::to_string(zeros) + ";");
    w("");
    std::vector<Key> keys;
    for (const auto& item : dbl.items) {
        if (item.first.kind == "g7") keys.push_back(item.first);
    }
    std::sort(keys.begin(), keys.end());   // sorted(k for k in double if k[0] == "g7"): the set ('P' before 'S'), then the index
    std::size_t published = 0;
    std::size_t sweep = 0;
    for (const Key& k : keys) {
        if (k.set == "P") ++published;
        if (k.set == "S") ++sweep;
    }
    w("inline constexpr std::size_t kPublishedCases = " + std::to_string(published) + ";");
    w("inline constexpr std::size_t kSweepCases     = " + std::to_string(sweep) + ";");
    w("");
    w("inline constexpr std::array<Record, " + std::to_string(keys.size()) + "> kValues{{");
    for (const Key& k : keys) {
        const std::vector<double>& v = *dbl.find(k);
        const std::vector<double>* rho_record = dbl.find(Key{"g7d", k.set, k.index});
        if (rho_record == nullptr || rho_record->empty()) throw BadInput("no GTD7D record for " + k.set + std::to_string(k.index));
        std::string nums;
        for (std::size_t i = 0; i < v.size(); ++i) nums += (i != 0 ? ", " : "") + dk::py_format_e(v[i], 17);
        w("    {'" + k.set + "', " + dk::pad_left(std::to_string(k.index), 3) + ", {" + nums + "}, " + dk::py_format_e((*rho_record)[0], 17) + "},");
    }
    w("}};");
    w("");
    w("}  // namespace odl::atmosphere::reference");
    std::string text;
    for (const std::string& line : L) {
        text += line;
        text += '\n';
    }
    return text;
}

std::string which(const std::string& name, const std::string& path) {
    if (name.find('/') != std::string::npos) return {};   // shutil.which treats a name with a directory part as a path of its own: not used here
    std::set<std::string> seen;
    std::size_t from = 0;
    for (;;) {
        const std::size_t colon = path.find(':', from);
        const std::string dir = path.substr(from, colon == std::string::npos ? std::string::npos : colon - from);
        if (seen.insert(dir).second) {
            const std::string candidate = dir.empty() ? name : dir + "/" + name;
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) && ::access(candidate.c_str(), X_OK) == 0 && !std::filesystem::is_directory(candidate, ec)) return candidate;
        }
        if (colon == std::string::npos) return {};
        from = colon + 1;
    }
}

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.root = default_root();
    return settings;
}

namespace {

const char kUsageText[] = "usage: msis_reference [-h] [--root ROOT] [--out OUT] [--check] [--work WORK] [--sweep-input SWEEP_INPUT]\n";

const char kHelpText[] =
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

// The first line of stdout, as  subprocess.run([prog, "--version"], capture_output=True, text=True).stdout.split("\n")[0]
std::string version_line(const std::string& program) {
    dk::ProcessResult r;
    try {
        r = dk::run_process({program, "--version"});
    } catch (const std::runtime_error& exc) {
        throw BadInput("cannot run " + program + ": " + exc.what());
    }
    const std::string out = dk::universal_newlines(r.out);
    return out.substr(0, out.find('\n'));
}

// find_source(): the cached Fortran, verified
std::filesystem::path find_source(const std::filesystem::path& root) {
    const std::filesystem::path manifest_path = root / "manifest" / "manifest.json";
    dk::Json doc;
    try {
        doc = dk::Json::parse(dk::read_text(manifest_path));
    } catch (const dk::JsonError& exc) {
        throw BadInput(manifest_path.string() + ": " + exc.reason + ": line " + std::to_string(exc.line) + " column " + std::to_string(exc.column));
    }
    const dk::Json* entries = doc.find("entries");
    const dk::Json* cache = doc.find("cache");
    if (entries == nullptr || !entries->is_array() || cache == nullptr || !cache->is_string()) throw BadInput(manifest_path.string() + ": no \"entries\" list or no \"cache\" string");
    const dk::Json* entry = nullptr;
    for (const dk::Json& e : entries->as_array()) {
        const dk::Json* id = e.is_object() ? e.find("id") : nullptr;
        if (id == nullptr || !id->is_string()) throw BadInput(manifest_path.string() + ": an entry without an id");
        if (id->as_string() == kSourceId) {
            entry = &e;
            break;
        }
    }
    if (entry == nullptr) throw BadInput(manifest_path.string() + ": no entry " + kSourceId);
    const dk::Json* filename = entry->find("filename");
    const dk::Json* pinned = entry->find("sha256");
    if (filename == nullptr || !filename->is_string() || pinned == nullptr || !pinned->is_string()) throw BadInput(std::string(kSourceId) + ": no \"filename\" or no \"sha256\" in the manifest");
    const std::filesystem::path p = root / cache->as_string() / kSourceId / filename->as_string();
    const std::string got = dk::sha256_hex(dk::read_bytes(p));
    if (got != pinned->as_string()) throw Fatal(std::string(kSourceId) + ": cached bytes are not the pinned bytes\n  want " + pinned->as_string() + "\n  got  " + got);
    return p;
}

// build(): model.f, driver.f and the executable
std::filesystem::path build(const std::string& gfortran, const std::filesystem::path& src, const std::filesystem::path& work, bool promoted) {
    const std::filesystem::path model = work / "model.f";
    dk::write_text(model, model_source(dk::read_text_lossy(src)));
    const std::filesystem::path drv = work / "driver.f";
    dk::write_text(drv, driver_source());
    const std::filesystem::path exe = work / (promoted ? "ref_d" : "ref_s");
    std::vector<std::string> cmd = {gfortran};
    for (const std::string& f : flags()) cmd.push_back(f);
    if (promoted) cmd.emplace_back(kPromote);
    cmd.emplace_back("-o");
    cmd.push_back(exe.string());
    cmd.push_back(model.string());
    cmd.push_back(drv.string());
    dk::ProcessResult r;
    try {
        r = dk::run_process(cmd);
    } catch (const std::runtime_error& exc) {
        throw BadInput("cannot run " + gfortran + ": " + exc.what());
    }
    if (r.exit_code != 0) throw Fatal("gfortran failed:\n" + tail_of(r.err, 2000));
    return exe;
}

// run(): the sweep on stdin, the reference's output parsed
Results run_reference(const std::filesystem::path& exe, const std::string& input) {
    dk::ProcessOptions options;
    options.input = input;
    dk::ProcessResult r;
    try {
        r = dk::run_process({exe.string()}, options);
    } catch (const std::runtime_error& exc) {
        throw BadInput("cannot run " + exe.string() + ": " + exc.what());
    }
    if (r.exit_code != 0) throw Fatal("reference run failed: " + tail_of(r.err, 1000));
    return parse_output(r.out);
}

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        std::filesystem::path root = settings.root;
        std::filesystem::path work = settings.work;
        std::string out_given = kDefaultOut;
        std::optional<std::string> sweep_file;
        bool check = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--root" || opt == "--out" || opt == "--work" || opt == "--sweep-input") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--root") root = value;
                else if (opt == "--out") out_given = value;
                else if (opt == "--work") work = value;
                else sweep_file = value;
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        try {
            // GFORTRAN = next((c for c in ("gfortran", ...) if shutil.which(c)), None)
            std::string search_path;
            if (settings.path) {
                search_path = *settings.path;
            } else if (const char* env = std::getenv("PATH")) {
                search_path = env;
            }
            std::string gfortran;
            std::string gfortran_path;
            for (const std::string& c : settings.compilers) {
                gfortran_path = which(c, search_path);
                if (!gfortran_path.empty()) {
                    gfortran = c;
                    break;
                }
            }
            if (gfortran.empty()) {
                io.out << "no Fortran compiler on PATH; nothing regenerated (ATMO-R-028: the gate does not need one)\n";
                return kOk;
            }
            const std::string ver = version_line(gfortran_path);
            const std::filesystem::path src = find_source(root);
            const std::string sha = dk::sha256_hex(dk::read_bytes(src));
            if (work.empty()) work = root / "tools" / ".msisref";
            std::filesystem::create_directories(work);
            const std::string input = sweep_input();
            if (sweep_file) dk::write_text(*sweep_file, input);
            const Results single = run_reference(build(gfortran_path, src, work, false), input);
            const Results dbl = run_reference(build(gfortran_path, src, work, true), input);

            // The threshold-sensitivity claim (class A's worst barely moves across three decades of the materiality threshold) is measured here, each time, rather than quoted from whenever it was last checked by hand.
            double sensitivity[3] = {};
            const double thresholds[3] = {1e-12, 1e-15, 1e-20};
            for (int k = 0; k < 3; ++k) {
                const Classes c = classify(single, dbl, thresholds[k]);
                if (c.material.empty()) throw BadInput("no material comparison at the threshold " + dk::py_format_g(thresholds[k]));
                sensitivity[k] = c.material.front().relative;
            }
            const Classes classes = classify(single, dbl);
            const std::string text = emit(ver, sha, classes, dbl, sensitivity);
            const std::filesystem::path out = root / out_given;
            if (check) {
                const std::string cur = std::filesystem::exists(out) ? dk::universal_newlines(dk::read_text(out)) : std::string();
                if (cur != text) {
                    io.err << "REGENERATED OUTPUT DIFFERS from the committed file\n";
                    return kFailed;
                }
                io.out << "ok  " << out_given << " reproduces from " << kSourceId << " with " << gfortran << '\n';
                return kOk;
            }
            dk::write_text(out, text);
            io.out << "wrote " << out_given << ": " << dbl.items.size() / 2 << " reference records; comparisons material " << classes.material.size() << ", immaterial " << classes.immaterial.size()
                   << ", underflowed " << classes.underflowed.size() << ", zero " << classes.zeros << '\n';
            return kOk;
        } catch (const Fatal& exc) {
            io.err << exc.what() << '\n';
            return kFailed;
        } catch (const BadInput& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        } catch (const std::runtime_error& exc) {   // dk::read_bytes, read_text, write_text, create_directories: the path and the reason
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::msis_reference

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::msis_reference::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
