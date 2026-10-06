// registry_tests.cpp — SPEC-measmod.md §8.1, MEAS-A-001 … MEAS-A-010 (the station and site registry).
//
// The registry is tested on three kinds of input: hand-built SINEX fixtures whose rows are QUOTED AS FACTS from the pinned
// files (SPEC-measmod.md ruling R3: a few rows, no file vendored), synthetic rows built to the same column layout where a
// closed form is wanted (the marker at a position the test chooses), and the real pinned files themselves, read through
// the manifest's own cache path. Expected numbers for the real station come from tools/measmod_reference.py (an
// iterative geodetic solution, not ERFA's), and the counts of the pinned release from tools/measmod_registry_facts.py.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/io/sinex.hpp>
#include <odl/measmod/registry.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include "measmod_reference.hpp"

#include <erfa.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace odl;
using namespace odl::measmod;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::TimeScale;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

constexpr double kPi = 3.14159265358979323846;

namespace {

std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const odl::time::LeapTable& leaps() {
    static const odl::time::LeapTable t = [] {
        auto r = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

Epoch utc(int y, int mo, int d, int h, int mi, double s) {
    auto e = Epoch::from_calendar(TimeScale::UTC, Calendar{y, mo, d, h, mi, s}, leaps());
    REQUIRE(e.has_value());
    return *e;
}

odl::io::SinexFile parse_snx(const std::string& text) {
    auto f = odl::io::read_sinex(text);
    REQUIRE(f.has_value());
    return *f;
}

// ---- SINEX fixtures ---------------------------------------------------------------------------------------------------
// A whole file from a header line and (block name, data lines) pairs.
std::string snx(const std::string& header, const std::vector<std::pair<std::string, std::string>>& blocks) {
    std::string t = header + "\n*-------------------------------------------------------------------------------\n";
    for (const auto& [name, lines] : blocks) t += "+" + name + "\n" + lines + "-" + name + "\n";
    t += "%ENDSNX\n";
    return t;
}

const char* kSlrfHeader = "%=SNX 2.02 GSC 26:036:43200 GSC 79:329:00000 00:000:00000 C 01422 2 S";
const char* kEccHeader = "%=SNX 2.02 GSC 26:036:12000 GSC 68:041:00000 00:000:00000 L 00558 0 X";

// Rows quoted as FACTS from the pinned SLRF2020 release 2026.02.05 (Yarragadee 7090 A, Monument Peak 7110 A) and the
// ILRS eccentricity file of 2026-05-27. Copied verbatim, leading space (SINEX's data-line marker) included.
const char* kSlrfSiteId =
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900511\n"
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900512\n"
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900513\n"
    " 7110  A 40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100411\n"
    " 7110  A 40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100412\n";
const char* kSlrfEpochs =
    " 7090  A    1 C 83:011:70773 00:000:00000 02:007:23100\n"
    " 7110  A    1 C 82:362:51341 99:289:56376 91:143:53858\n"
    " 7110  A    2 C 99:290:00238 10:099:51758 05:011:69198\n";
const char* kSlrfEstimates =
    "   211 STAX   7090  A    1 15:001:00000 m    2 -.238900777005032E+07 0.69543E-03\n"
    "   212 STAY   7090  A    1 15:001:00000 m    2 0.504332948581405E+07 0.59590E-03\n"
    "   213 STAZ   7090  A    1 15:001:00000 m    2 -.307852397088064E+07 0.69866E-03\n"
    "   214 VELX   7090  A    1 15:001:00000 m/y  2 -.468239683104311E-01 0.52896E-04\n"
    "   215 VELY   7090  A    1 15:001:00000 m/y  2 0.797007645988301E-02 0.45572E-04\n"
    "   216 VELZ   7090  A    1 15:001:00000 m/y  2 0.509599889854529E-01 0.59966E-04\n"
    "   295 STAX   7110  A    1 15:001:00000 m    2 -.238627875112565E+07 0.32876E-02\n"
    "   296 STAY   7110  A    1 15:001:00000 m    2 -.480235365861609E+07 0.33993E-02\n"
    "   297 STAZ   7110  A    1 15:001:00000 m    2 0.344488186410130E+07 0.32123E-02\n"
    "   298 VELX   7110  A    1 15:001:00000 m/y  2 -.299719550092641E-01 0.17952E-03\n"
    "   299 VELY   7110  A    1 15:001:00000 m/y  2 0.270740929749646E-01 0.18559E-03\n"
    "   300 VELZ   7110  A    1 15:001:00000 m/y  2 0.145845601135634E-01 0.17888E-03\n"
    "   301 STAX   7110  A    2 15:001:00000 m    2 -.238627876266935E+07 0.16895E-02\n"
    "   302 STAY   7110  A    2 15:001:00000 m    2 -.480235368593289E+07 0.16161E-02\n"
    "   303 STAZ   7110  A    2 15:001:00000 m    2 0.344488186381700E+07 0.15264E-02\n"
    "   304 VELX   7110  A    2 15:001:00000 m/y  2 -.307701304820486E-01 0.14389E-03\n"
    "   305 VELY   7110  A    2 15:001:00000 m/y  2 0.254089802825626E-01 0.13681E-03\n"
    "   306 VELZ   7110  A    2 15:001:00000 m/y  2 0.139238733135981E-01 0.13154E-03\n";
const char* kEccSiteId =
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900511\n"
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900512\n"
    " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900513\n"
    " 7110  A 40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100411\n"
    " 7110  A 40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100412\n";
const char* kEccRows =
    " 7090  A    1 L 92:021:00000 92:083:86399 UNE   3.1813  -0.0096   0.0192        70900511\n"
    " 7090  A    1 L 92:084:00000 92:195:86399 UNE   3.1813  -0.0096   0.0192        70900512\n"
    " 7090  A    1 L 92:203:00000 98:233:86399 UNE   3.1813  -0.0096   0.0192        70900513\n"
    " 7090  A    1 L 98:234:00000 03:330:86399 UNE   3.1809  -0.0083   0.0178        70900513\n"
    " 7090  A    1 L 03:331:00000 07:150:86399 UNE   3.1821  -0.0083   0.0184        70900513\n"
    " 7090  A    1 L 07:151:00000 10:195:86399 UNE   3.1823  -0.0062   0.0190        70900513\n"
    " 7090  A    1 L 10:196:00000 14:079:86399 UNE   3.1820  -0.0068   0.0164        70900513\n"
    " 7090  A    1 L 14:080:00000 00:000:00000 UNE   3.1827  -0.0064   0.0194        70900513\n"
    " 7110  A    1 L 92:122:00000 03:099:86399 UNE   3.1890  -0.0260  -0.0190        71100411\n"
    " 7110  A    1 L 03:143:00000 11:316:86399 UNE   3.1880  -0.0213  -0.0208        71100412\n"
    " 7110  A    1 L 11:317:00000 18:136:86399 UNE   3.1900  -0.0260  -0.0180        71100412\n"
    " 7110  A    1 L 18:137:00000 00:000:00000 UNE   3.1895  -0.0242  -0.0148        71100412\n";
const char* kPsd =
    " 7110  A 40497M001 10:094:81643 E 1   -3.67  0.0467                     SLR\n"
    "                                N 0\n"
    "                                U 0\n";

std::string slrf_fixture(const std::string& site_id = kSlrfSiteId, const std::string& epochs = kSlrfEpochs,
                         const std::string& estimates = kSlrfEstimates) {
    return snx(kSlrfHeader, {{"FILE/REFERENCE", " DESCRIPTION        Expanded set of SLR stations in ITRF2020 frame\n VERSION            260205\n"},
                             {"SITE/ID", site_id},
                             {"SOLUTION/EPOCHS", epochs},
                             {"SOLUTION/ESTIMATE", estimates}});
}
std::string ecc_fixture(const std::string& site_id = kEccSiteId, const std::string& rows = kEccRows) {
    return snx(kEccHeader, {{"FILE/REFERENCE", " DESCRIPTION        UNE 250513 ILRS eccentricities file\n"},
                            {"SITE/ID", site_id},
                            {"SITE/ECCENTRICITY", rows}});
}

const RegistrySources kSources{"slrf-sha-test", "ecc-sha-test", "psd-sha-test"};

Result<SlrRegistry, MeasError> build_from(const std::string& slrf, const std::string& ecc, const std::string& psd = kPsd) {
    return SlrRegistry::build(parse_snx(slrf), parse_snx(ecc), psd, leaps(), kSources);
}

SlrRegistry fixture_registry() {
    auto r = build_from(slrf_fixture(), ecc_fixture());
    REQUIRE(r.has_value());
    return *r;
}

const SlrRegistry& real_registry() {
    static const SlrRegistry r = [] {
        const std::string slrf = slurp(ODL_SLRF2020_FILE), ecc = slurp(ODL_SLR_ECC_FILE), psd = slurp(ODL_SLR_PSD_FILE);
        auto a = odl::io::read_sinex(slrf);
        REQUIRE(a.has_value());
        auto b = odl::io::read_sinex(ecc);
        REQUIRE(b.has_value());
        auto reg = SlrRegistry::build(*a, *b, psd, leaps(), RegistrySources{ODL_SLRF2020_SHA256, ODL_SLR_ECC_SHA256, ODL_SLR_PSD_SHA256});
        if (!reg) FAIL("the real pinned files did not build: " << reg.error().id << " " << reg.error().message);
        return *reg;
    }();
    return r;
}

SodKey sod(long v) {
    auto k = SodKey::from_sod(v);
    REQUIRE(k.has_value());
    return *k;
}

// ---- synthetic rows, to the pinned files' column layout -------------------------------------------------------------------
std::string row_site_id(int pad, char pt, const char* domes, const char* name, long sod_v) {
    char b[160];
    std::snprintf(b, sizeof b, " %4d  %c %-9s L %-22s  000 00 00.0  00 00 00.0      0.0     %08ld\n", pad, pt, domes, name, sod_v);
    return b;
}
std::string row_epochs(int pad, char pt, int soln, const char* a, const char* b_, const char* mean) {
    char b[160];
    std::snprintf(b, sizeof b, " %4d  %c %4d C %s %s %s\n", pad, pt, soln, a, b_, mean);
    return b;
}
std::string rows_estimate(int pad, char pt, int soln, double x, double y, double z, double vx, double vy, double vz, int& idx) {
    std::string out;
    const char* type[6] = {"STAX", "STAY", "STAZ", "VELX", "VELY", "VELZ"};
    const double v[6] = {x, y, z, vx, vy, vz};
    for (int i = 0; i < 6; ++i) {
        char val[40];
        std::snprintf(val, sizeof val, "%.14E", v[i]);
        char b[200];
        std::snprintf(b, sizeof b, "%6d %-4s   %4d  %c %4d %-12s %-4s %c %21s %11s\n", idx++, type[i], pad, pt, soln, "15:001:00000",
                      i < 3 ? "m" : "m/y", '2', val, "0.10000E-02");
        out += b;
    }
    return out;
}
std::string row_ecc(int pad, char pt, int soln, const char* a, const char* b_, double u, double n, double e, long sod_v) {
    char b[200];
    std::snprintf(b, sizeof b, " %4d  %c %4d L %-12s %-12s UNE%9.4f%9.4f%9.4f        %08ld\n", pad, pt, soln, a, b_, u, n, e, sod_v);
    return b;
}

// the WGS 84 ellipsoid, written here and not borrowed from ERFA (SPEC-measmod MEAS-A-003)
constexpr double kA = 6378137.0, kF = 1.0 / 298.257223563;
odl::Vec3 geodetic_to_xyz(double lat, double lon, double h) {
    const double e2 = kF * (2 - kF), s = std::sin(lat), c = std::cos(lat);
    const double n = kA / std::sqrt(1 - e2 * s * s);
    return {(n + h) * c * std::cos(lon), (n + h) * c * std::sin(lon), (n * (1 - e2) + h) * s};
}

bool near(const odl::Vec3& a, const odl::Vec3& b, double tol) { return (a - b).norm() <= tol; }

}  // namespace

// ========================================================================================================================
TEST_CASE("MEAS-A-001  build from hand-built SINEX fixtures whose rows are quoted as facts from the pinned files reproduces pads, "
          "SODs, markers, spans and eccentricities, and records both headers' own version strings and the supplied hashes",
          "[measmod][registry]") {
    const SlrRegistry r = fixture_registry();
    CHECK(r.pad_count() == 2);
    CHECK(r.placed_sod_count() == 5);
    CHECK(r.unplaced_sod_count() == 0);
    CHECK(r.post_seismic_event_count() == 1);
    CHECK_THAT(r.slrf_release(), ContainsSubstring("VERSION 260205"));
    CHECK_THAT(r.slrf_release(), ContainsSubstring("26:036:43200"));
    CHECK_THAT(r.ecc_release(), ContainsSubstring("UNE 250513 ILRS eccentricities file"));
    CHECK_THAT(r.ecc_release(), ContainsSubstring("26:036:12000"));
    CHECK(r.sources().slrf_sha256 == "slrf-sha-test");
    CHECK(r.sources().ecc_sha256 == "ecc-sha-test");
    CHECK(r.sources().psd_sha256 == "psd-sha-test");

    // the 18-row eccentricity history of Yarragadee (here: the six rows the fixture quotes) selects by span
    auto s1 = r.site(sod(70900513), utc(1995, 6, 1, 0, 0, 0));   // span 92:203 .. 98:233
    REQUIRE(s1.has_value());
    CHECK(s1->eccentricity_une_m.x == 3.1813);
    CHECK(s1->eccentricity_une_m.y == -0.0096);
    CHECK(s1->eccentricity_une_m.z == 0.0192);
    auto s2 = r.site(sod(70900513), utc(2020, 6, 15, 0, 0, 0));  // span 14:080 .. open
    REQUIRE(s2.has_value());
    CHECK(s2->eccentricity_une_m.x == 3.1827);
    CHECK(s2->eccentricity_une_m.z == 0.0194);
    CHECK(s2->pad == 7090);
    CHECK(s2->point == 'A');
    CHECK(s2->domes == "50107M001");
    CHECK(s2->name == "Yarragadee MOBLAS-5");
    CHECK(s2->sod == SodKey{7090, 5, 13});
    CHECK_FALSE(s2->post_seismic_override_used);
    // the build is reproducible: a second build from the same text gives the same site
    auto again = build_from(slrf_fixture(), ecc_fixture());
    REQUIRE(again.has_value());
    auto s3 = again->site(sod(70900513), utc(2020, 6, 15, 0, 0, 0));
    REQUIRE(s3.has_value());
    CHECK(near(s3->srp_itrs_m, s2->srp_itrs_m, 0.0));
}

TEST_CASE("MEAS-A-002  the marker position x_ref + v (t - t_ref) with the year of 365.25 days, the SINEX epoch format (49 is 2049, 50 is 1950, "
          "00:000:00000 open), the solution of the span that contains t, and the day-end convention",
          "[measmod][registry]") {
    // ---- the SINEX epoch format (SPEC-measmod §3.6) ----------------------------------------------------------------
    auto d = decode_sinex_epoch("49:365:86399");
    REQUIRE(d.has_value());
    CHECK((d->year == 2049 && d->day_of_year == 365 && d->seconds_of_day == 86399 && !d->unset));
    d = decode_sinex_epoch("50:001:00000");
    REQUIRE(d.has_value());
    CHECK(d->year == 1950);
    d = decode_sinex_epoch("99:365:86399");
    REQUIRE(d.has_value());
    CHECK(d->year == 1999);
    d = decode_sinex_epoch("26:036:43200");   // the pinned file's own creation stamp: 2026-02-05 12:00:00 UTC
    REQUIRE(d.has_value());
    CHECK((d->year == 2026 && d->day_of_year == 36 && d->seconds_of_day == 43200));
    d = decode_sinex_epoch("00:000:00000");
    REQUIRE(d.has_value());
    CHECK(d->unset);
    for (const char* bad : {"26:036:4320", "2026:036:43200", "26-036-43200", "26:367:00000", "26:000:00000", "25:366:00000", "26:036:86400", "ab:cd:efghi"}) {
        auto r = decode_sinex_epoch(bad);
        INFO(bad);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-020");
    }
    // 2024 has a day 366; 2025 does not
    CHECK(decode_sinex_epoch("24:366:00000").has_value());

    // ---- the position formula, on synthetic rows with round numbers --------------------------------------------------
    int idx = 1;
    // marker 9001 A: one solution from 2000:001 to open, reference epoch 15:001:00000 (2015-01-01 00:00:00 UTC), 1 m/yr along x
    const std::string est = rows_estimate(9001, 'A', 1, 6378137.0, 0.0, 0.0, 1.0, 0.0, 0.0, idx);
    // …and a span before the UTC era (1971-75) which never matches, plus a second SOD whose span STARTS before 1972 and ends in 2005
    // (unbounded below, as the pinned file's 71:001:00000 is)
    const std::string slrf2 = slrf_fixture(row_site_id(9001, 'A', "99999M001", "Synthetic", 90010101) + row_site_id(9001, 'A', "99999M001", "Synthetic", 90010102),
                                           row_epochs(9001, 'A', 1, "00:001:00000", "00:000:00000", "10:001:00000"), est);
    const std::string ecc = ecc_fixture(row_site_id(9001, 'A', "99999M001", "Synthetic", 90010101) + row_site_id(9001, 'A', "99999M001", "Synthetic", 90010102),
                                        row_ecc(9001, 'A', 1, "71:001:00000", "75:365:86399", 9.0, 9.0, 9.0, 90010101) +
                                            row_ecc(9001, 'A', 1, "00:001:00000", "00:000:00000", 0.0, 0.0, 0.0, 90010101) +
                                            row_ecc(9001, 'A', 1, "71:001:00000", "05:365:86399", 7.0, 8.0, 9.0, 90010102));
    auto r = build_from(slrf2, ecc);
    REQUIRE(r.has_value());
    {   // the pre-era span never leaks into a supported epoch; the one that starts before the era is unbounded below
        auto pre = r->site(sod(90010101), utc(2020, 1, 1, 0, 0, 0));
        REQUIRE(pre.has_value());
        CHECK(near(pre->eccentricity_une_m, {0.0, 0.0, 0.0}, 0.0));
        auto open_below = r->site(sod(90010102), utc(2003, 6, 1, 0, 0, 0));
        REQUIRE(open_below.has_value());
        CHECK(near(open_below->eccentricity_une_m, {7.0, 8.0, 9.0}, 0.0));
        auto after = r->site(sod(90010102), utc(2006, 1, 1, 0, 0, 0));            // 05:365:86399 ended on 2005-12-31
        REQUIRE_FALSE(after.has_value());
        CHECK(after.error().id == "MEAS-F-002");
    }
    const Epoch t_ref = utc(2015, 1, 1, 0, 0, 0);
    const double year_s = 365.25 * 86400.0;
    auto at = [&](double seconds_after_ref) {
        auto s = r->site(sod(90010101), t_ref.add(odl::time::Duration::from_seconds(seconds_after_ref)));
        REQUIRE(s.has_value());
        return *s;
    };
    CHECK(near(at(0.0).marker_itrs_m, {6378137.0, 0.0, 0.0}, 1e-9));
    CHECK(near(at(10 * year_s).marker_itrs_m, {6378137.0 + 10.0, 0.0, 0.0}, 1e-9));          // 10 x 365.25 d: exactly 10 m
    CHECK(near(at(1.0).marker_itrs_m, {6378137.0 + 1.0 / 31557600.0, 0.0, 0.0}, 1e-9));      // 1 s: v / 31 557 600
    CHECK(near(at(-year_s).marker_itrs_m, {6378137.0 - 1.0, 0.0, 0.0}, 1e-9));                // before the reference epoch too

    // ---- the span that contains t: 1 s inside accepted, 1 s outside refused; gaps, before, after --------------------
    // 7110 A has solution 1 to 99:289:56376 (1999-10-16 15:39:36) and solution 2 from 99:290:00238 (1999-10-17 00:03:58)
    const SlrRegistry f = fixture_registry();
    CHECK(f.site(sod(71100411), utc(1999, 10, 16, 15, 39, 36)).has_value());          // the end, inclusive
    auto in_gap = f.site(sod(71100411), utc(1999, 10, 16, 20, 0, 0));
    REQUIRE_FALSE(in_gap.has_value());
    CHECK(in_gap.error().id == "MEAS-F-003");
    CHECK(f.site(sod(71100411), utc(1999, 10, 17, 0, 3, 58)).has_value());            // the start of solution 2, inclusive
    auto gap2 = f.site(sod(71100411), utc(1999, 10, 17, 0, 3, 57));
    REQUIRE_FALSE(gap2.has_value());
    CHECK(gap2.error().id == "MEAS-F-003");
    // a different solution of one marker is a different position: the two solutions differ by 3 m at the same instant
    // (the fixture's solutions 1 and 2 are the real ones, which differ by millimetres to a metre)
    CHECK(f.site(sod(71100412), utc(2010, 4, 4, 22, 40, 42)).has_value());
    auto after_last = f.site(sod(71100412), utc(2010, 4, 10, 0, 0, 0));              // solution 2 ended 10:099:51758; the fixture has no solution 3
    REQUIRE_FALSE(after_last.has_value());
    CHECK(after_last.error().id == "MEAS-F-003");

    // ---- the day-end convention: `98:233:86399` ends the span at the NEXT midnight, so the last second is inside --------
    const SlrRegistry y = fixture_registry();
    CHECK(y.site(sod(70900513), utc(1998, 8, 21, 23, 59, 59.5)).has_value());         // 98:233 is 1998-08-21; 23:59:59.5 is inside its span
    CHECK(y.site(sod(70900513), utc(1998, 8, 22, 0, 0, 0)).has_value());              // 98:234:00000 starts the next span
    auto before_first = y.site(sod(70900513), utc(1992, 7, 20, 23, 59, 59));          // one second before 92:203:00000
    REQUIRE_FALSE(before_first.has_value());
    CHECK(before_first.error().id == "MEAS-F-002");
    CHECK(y.site(sod(70900513), utc(1992, 7, 21, 0, 0, 0)).has_value());
}

TEST_CASE("MEAS-A-003  the system reference point marker + E (U, N, E): unit offsets move the point by exactly the local up, north and "
          "east of the GEODETIC normal, and the geocentric latitude is shown failing",
          "[measmod][registry]") {
    struct Site { int pad; double lat, lon, h; long sod_v; };
    const double deg = kPi / 180.0;
    const Site sites[] = {{9101, 0.0, 0.0, 0.0, 91010101}, {9102, 0.0, 90.0 * deg, 0.0, 91020101}, {9103, 90.0 * deg, 0.0, 0.0, 91030101},
                          {9104, 45.0 * deg, 30.0 * deg, 100.0, 91040101}};
    std::string ids_s, ids_e, ep, est, ecc_rows;
    int idx = 1;
    for (const auto& s : sites) {
        const odl::Vec3 p = geodetic_to_xyz(s.lat, s.lon, s.h);
        ids_s += row_site_id(s.pad, 'A', "99999M001", "Synthetic", s.sod_v);
        ep += row_epochs(s.pad, 'A', 1, "00:001:00000", "00:000:00000", "10:001:00000");
        est += rows_estimate(s.pad, 'A', 1, p.x, p.y, p.z, 0.0, 0.0, 0.0, idx);
        ecc_rows += row_ecc(s.pad, 'A', 1, "00:001:00000", "00:000:00000", 3.185, 2.0, 3.0, s.sod_v);
    }
    auto r = build_from(slrf_fixture(ids_s, ep, est), ecc_fixture(ids_s, ecc_rows));
    REQUIRE(r.has_value());
    const Epoch when = utc(2020, 1, 1, 0, 0, 0);
    for (const auto& s : sites) {
        auto site = r->site(sod(s.sod_v), when);
        REQUIRE(site.has_value());
        const double sl = std::sin(s.lon), cl = std::cos(s.lon), sp = std::sin(s.lat), cp = std::cos(s.lat);
        const odl::Vec3 up{cp * cl, cp * sl, sp}, north{-sp * cl, -sp * sl, cp}, east{-sl, cl, 0.0};
        const odl::Vec3 expected = up * 3.185 + north * 2.0 + east * 3.0;
        INFO("latitude " << s.lat / deg << " deg");
        CHECK(near(site->srp_itrs_m - site->marker_itrs_m, expected, 1e-8));
        CHECK_THAT(site->marker_geodetic.latitude_rad, WithinAbs(s.lat, 1e-12));
        CHECK_THAT(site->marker_geodetic.height_m, WithinAbs(s.h, 1e-6));
    }
    // the closed-form corners: at (0, 0) up is +x, north +z, east +y: the offset is (3.185, 3, 2)
    auto eq = r->site(sod(91010101), when);
    REQUIRE(eq.has_value());
    CHECK(near(eq->srp_itrs_m - eq->marker_itrs_m, {3.185, 3.0, 2.0}, 1e-8));
    // Rule 5: with the GEOCENTRIC latitude the up offset points ~0.1924 deg away at geodetic 45 deg — 10.7 mm of 3.185 m — and
    // the same assertion fails for it
    auto mid = r->site(sod(91040101), when);
    REQUIRE(mid.has_value());
    const double lat = 45.0 * deg, lon = 30.0 * deg;
    const odl::Vec3 p = geodetic_to_xyz(lat, lon, 100.0);
    const double geocentric = std::atan2(p.z, std::hypot(p.x, p.y));
    const double sl = std::sin(lon), cl = std::cos(lon), sp = std::sin(geocentric), cp = std::cos(geocentric);
    const odl::Vec3 wrong = odl::Vec3{cp * cl, cp * sl, sp} * 3.185 + odl::Vec3{-sp * cl, -sp * sl, cp} * 2.0 + odl::Vec3{-sl, cl, 0.0} * 3.0;
    CHECK_FALSE(near(mid->srp_itrs_m - mid->marker_itrs_m, wrong, 1e-3));
    const double tilt = (mid->srp_itrs_m - mid->marker_itrs_m - wrong).norm();
    // the two rotations differ by the angle between the geodetic and the geocentric latitude (0.1924 deg = 3.358 mrad), about the east
    // axis, acting on the up and north offsets: 3.358 mrad x hypot(3.185, 2) m = 12.6 mm (10.7 mm of it from the 3.185 m up offset alone)
    CHECK_THAT(tilt, WithinAbs((lat - geocentric) * std::hypot(3.185, 2.0), 1e-6));
    CHECK(tilt > 0.010);
}

TEST_CASE("MEAS-A-008  PadId, SodKey and OpticalStationNumber are three distinct types: none converts to or is constructible from another, "
          "and the registry's lookup accepts only a SodKey",
          "[measmod][registry]") {
    STATIC_REQUIRE_FALSE(std::is_convertible_v<PadId, SodKey>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<SodKey, PadId>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<PadId, OpticalStationNumber>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<OpticalStationNumber, PadId>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<OpticalStationNumber, SodKey>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<int, PadId>);                 // a bare number is not a pad
    STATIC_REQUIRE_FALSE(std::is_convertible_v<int, OpticalStationNumber>);  // nor an optical station
    STATIC_REQUIRE_FALSE(std::is_constructible_v<PadId, OpticalStationNumber>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<OpticalStationNumber, PadId>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<SodKey, PadId>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<SodKey, int>);
    // the lookup takes a SodKey and nothing else
    STATIC_REQUIRE(std::is_invocable_v<decltype(&SlrRegistry::site), const SlrRegistry&, SodKey, const Epoch&, SiteOptions>);
    STATIC_REQUIRE_FALSE(std::is_invocable_v<decltype(&SlrRegistry::site), const SlrRegistry&, PadId, const Epoch&, SiteOptions>);
    STATIC_REQUIRE_FALSE(std::is_invocable_v<decltype(&SlrRegistry::site), const SlrRegistry&, OpticalStationNumber, const Epoch&, SiteOptions>);
    STATIC_REQUIRE_FALSE(std::is_invocable_v<decltype(&SlrRegistry::site), const SlrRegistry&, long, const Epoch&, SiteOptions>);
    // and the SOD arithmetic: the real file's `h2 YARL 7090 5 13 3` is 70900513
    auto k = SodKey::make(7090, 5, 13);
    REQUIRE(k.has_value());
    CHECK(k->sod() == 70900513);
    auto back = SodKey::from_sod(70900513);
    REQUIRE(back.has_value());
    CHECK(*back == *k);
    for (auto bad : {SodKey::make(0, 5, 13), SodKey::make(10000, 5, 13), SodKey::make(7090, 100, 13), SodKey::make(7090, 5, 100),
                     SodKey::make(7090, -1, 13), SodKey::from_sod(9999), SodKey::from_sod(100000000)}) {
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "MEAS-F-001");
    }
}

TEST_CASE("MEAS-A-006  the post-seismic refusal on the real events: refused at the event and after, the single per-call override is "
          "recorded, a station with no event is unaffected",
          "[measmod][registry]") {
    const SlrRegistry r = fixture_registry();
    // 7110 A, DOMES 40497M001: event 10:094:81643 = 2010-04-04 22:40:43 UTC; SOD 71100412 has an eccentricity span 03:143 .. 11:316
    const Epoch before = utc(2010, 4, 4, 22, 40, 42), at = utc(2010, 4, 4, 22, 40, 43), later = utc(2010, 4, 8, 12, 0, 0);
    auto b = r.site(sod(71100412), before);
    REQUIRE(b.has_value());
    CHECK_FALSE(b->post_seismic_override_used);
    for (const Epoch& t : {at, later}) {
        auto refused = r.site(sod(71100412), t);
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().id == "MEAS-F-004");
        CHECK_THAT(refused.error().message, ContainsSubstring("psd-sha-test"));            // the PSD file's hash is named
        CHECK_THAT(refused.error().message, ContainsSubstring("2010-04-04 22:40:43"));     // and the event's epoch
        CHECK_THAT(refused.error().message, ContainsSubstring("accept_linear_position_after_post_seismic_event"));
        SiteOptions o;
        o.accept_linear_position_after_post_seismic_event = true;
        auto allowed = r.site(sod(71100412), t, o);
        REQUIRE(allowed.has_value());
        CHECK(allowed->post_seismic_override_used);                                       // recorded in what is returned
        // the override is per call: the next call without it refuses again
        auto again = r.site(sod(71100412), t);
        REQUIRE_FALSE(again.has_value());
        CHECK(again.error().id == "MEAS-F-004");
    }
    // the same instants at Yarragadee (no event) are accepted and the flag is false
    for (const Epoch& t : {before, at, later}) {
        auto y = r.site(sod(70900513), t);
        REQUIRE(y.has_value());
        CHECK_FALSE(y->post_seismic_override_used);
    }
    // the override on a site with no event does not set the flag (it records what was actually overridden)
    SiteOptions o;
    o.accept_linear_position_after_post_seismic_event = true;
    auto y2 = r.site(sod(70900513), later, o);
    REQUIRE(y2.has_value());
    CHECK_FALSE(y2->post_seismic_override_used);

    // ---- on the real pinned data ----------------------------------------------------------------------------------
    {
        const SlrRegistry& real = real_registry();
        const Epoch t2026 = utc(2026, 1, 1, 0, 0, 0);
        for (long s : {71100412L, 72371901L, 74031306L, 78383603L}) {                // 7110, 7237, 7403, 7838
            auto e = real.site(sod(s), t2026);
            INFO(s);
            REQUIRE_FALSE(e.has_value());
            CHECK(e.error().id == "MEAS-F-004");
            SiteOptions ov;
            ov.accept_linear_position_after_post_seismic_event = true;
            auto ok = real.site(sod(s), t2026, ov);
            REQUIRE(ok.has_value());
            CHECK(ok->post_seismic_override_used);
        }
        for (long s : {73085001L, 73588901L, 74057904L}) {                           // 7308, 7358, 7405: no solution in 2026
            auto e = real.site(sod(s), t2026);
            INFO(s);
            REQUIRE_FALSE(e.has_value());
            CHECK(e.error().id == "MEAS-F-003");                                     // the order of the checks: solution before post-seismic
        }
        // Yarragadee, no event, accepted at the same instant with the flag false
        auto yar = real.site(sod(70900513), t2026);
        REQUIRE(yar.has_value());
        CHECK_FALSE(yar->post_seismic_override_used);
    }
}

TEST_CASE("MEAS-A-007  optical sites: add with a citation, a blank citation, a unit slip and a changed repeat are refused, an unknown "
          "number refuses, and no list is bundled",
          "[measmod][registry]") {
    OpticalRegistry reg;
    CHECK(reg.size() == 0);                                   // nothing is bundled
    auto none = reg.site(OpticalStationNumber{4171});
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().id == "MEAS-F-005");
    CHECK_THAT(none.error().message, ContainsSubstring("4171"));
    CHECK_THAT(none.error().message, ContainsSubstring("0 site"));

    CHECK(reg.add(OpticalStationNumber{4171}, 0.8, 1.2, 150.0, "the observer's own survey, 2025").has_value());
    CHECK(reg.size() == 1);
    auto s = reg.site(OpticalStationNumber{4171});
    REQUIRE(s.has_value());
    CHECK(s->location.latitude_rad == 0.8);
    CHECK(s->location.longitude_rad == 1.2);
    CHECK(s->location.height_m == 150.0);
    CHECK(s->citation == "the observer's own survey, 2025");

    for (const char* blank : {"", " ", "   ", "\t", "\n", " \t \n "}) {
        auto r = reg.add(OpticalStationNumber{4172}, 0.8, 1.2, 150.0, blank);
        INFO("citation [" << blank << "]");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-006");
    }
    CHECK(reg.size() == 1);                                   // nothing was added by a refusal
    // a latitude of 45 (degrees given as radians), a longitude of 400 and a height of 30 000 each refuse
    struct Bad { double lat, lon, h; };
    for (const Bad b : {Bad{45.0, 1.2, 150.0}, Bad{0.8, 400.0, 150.0}, Bad{0.8, 1.2, 30000.0}, Bad{0.8, 1.2, -2000.0}, Bad{-1.6, 1.2, 0.0},
                        Bad{0.8, -3.2, 0.0}, Bad{std::nan(""), 1.2, 0.0}}) {
        auto r = reg.add(OpticalStationNumber{4173}, b.lat, b.lon, b.h, "cited");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-024");
    }
    CHECK(reg.size() == 1);
    // the neighbours of those limits pass
    CHECK(reg.add(OpticalStationNumber{4174}, kPi / 2, -kPi, 10000.0, "cited").has_value());
    CHECK(reg.add(OpticalStationNumber{4175}, -kPi / 2, 2 * kPi, -1000.0, "cited").has_value());
    // the same number again: an identical repeat is accepted, a changed one is refused and the old position kept
    CHECK(reg.add(OpticalStationNumber{4171}, 0.8, 1.2, 150.0, "the observer's own survey, 2025").has_value());
    auto changed = reg.add(OpticalStationNumber{4171}, 0.9, 1.2, 150.0, "another source");
    REQUIRE_FALSE(changed.has_value());
    CHECK(changed.error().id == "MEAS-F-025");
    CHECK(reg.site(OpticalStationNumber{4171})->location.latitude_rad == 0.8);
}

TEST_CASE("MEAS-A-009  build refusals MEAS-F-020, each alone: a position without its velocity, a solution for an unlisted marker, an "
          "estimate with no epoch row, an eccentricity SOD absent from its own SITE/ID, a SOD whose DOMES differs, a conflicting repeat",
          "[measmod][registry]") {
    auto replace = [](std::string text, const std::string& from, const std::string& to) {
        const auto pos = text.find(from);
        REQUIRE(pos != std::string::npos);
        text.replace(pos, from.size(), to);
        return text;
    };
    auto expect_f020 = [&](const std::string& slrf, const std::string& ecc, const char* what, const char* contains) {
        INFO(what);
        auto r = build_from(slrf, ecc);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-020");
        CHECK_THAT(r.error().message, ContainsSubstring(contains));
    };
    const std::string good_slrf = slrf_fixture(), good_ecc = ecc_fixture();
    REQUIRE(build_from(good_slrf, good_ecc).has_value());

    // a position with no velocity: drop the VELZ row of Yarragadee
    {
        const std::string row = "   216 VELZ   7090  A    1 15:001:00000 m/y  2 0.509599889854529E-01 0.59966E-04\n";
        expect_f020(replace(good_slrf, row, ""), good_ecc, "no VELZ", "lacks one of its six estimates");
    }
    // a solution for a marker SITE/ID does not list: change the epoch row's code
    expect_f020(replace(good_slrf, " 7090  A    1 C 83:011:70773", " 7099  A    1 C 83:011:70773"), good_ecc, "unlisted marker", "SITE/ID does not list");
    // an estimate with no epoch row: drop Yarragadee's epoch row
    expect_f020(replace(good_slrf, " 7090  A    1 C 83:011:70773 00:000:00000 02:007:23100\n", ""), good_ecc, "no epoch row", "no SOLUTION/EPOCHS row");
    // an eccentricity SOD absent from the eccentricity file's own SITE/ID
    expect_f020(good_slrf, replace(good_ecc, " 7110  A 40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100412\n", ""),
                "ecc SOD absent", "which the file's own SITE/ID does not list");
    // a SOD whose DOMES number differs between the two files
    expect_f020(good_slrf, replace(good_ecc, "40497M001 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100412",
                                   "40497M002 L Monument P MOBLAS-4    243 34 38.3  32 53 30.2  1839.7     71100412"),
                "domes differs", "DOMES");
    // a SOD listed twice in one SITE/ID with different content
    expect_f020(good_slrf, replace(good_ecc, kEccSiteId, std::string(kEccSiteId) + " 7090  A 50107M009 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900511\n"),
                "conflicting repeat", "listed twice");
    // an unreadable number, a SOD that does not begin with its pad, an eccentricity of another type
    expect_f020(replace(good_slrf, "-.238900777005032E+07", "-.23890077700XXXXE+07 "), good_ecc, "bad number", "not a number");
    expect_f020(good_slrf, replace(good_ecc, "UNE   3.1827  -0.0064   0.0194", "XYZ   3.1827  -0.0064   0.0194"), "not UNE", "not UNE");
    expect_f020(replace(good_slrf, "70900513\n", "80900513\n"), good_ecc, "SOD against pad", "does not begin with the site code");
    // an empty post-seismic list would silently remove the protection
    {
        auto r = build_from(good_slrf, good_ecc, "");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-020");
        CHECK_THAT(r.error().message, ContainsSubstring("no event"));
    }
    // the identical repeat is ACCEPTED (the real eccentricity file lists SOD 71100301 twice, the two rows equal)
    CHECK(build_from(good_slrf, replace(good_ecc, kEccSiteId, std::string(kEccSiteId) + " 7090  A 50107M001 L Yarragadee MOBLAS-5    115 20 48.2 -29 -2-47.3   242.0     70900511\n")).has_value());
    // the REAL files build too: marker 7307 B is listed with no solution and SOD 71100301 is listed twice, and neither refuses
    CHECK(real_registry().pad_count() == 186);
}


TEST_CASE("MEAS-A-004  the pinned release: 186 pads, 482 placed and 60 unplaced SODs; every unplaced SOD refuses saying it has no "
          "coordinates; 184 + 2 = 186",
          "[measmod][registry][real]") {
    const SlrRegistry& r = real_registry();
    CHECK(r.pad_count() == 186);
    CHECK(r.placed_sod_count() == 482);
    CHECK(r.unplaced_sod_count() == 60);
    CHECK(r.post_seismic_event_count() == 12);
    CHECK_THAT(r.slrf_release(), ContainsSubstring("VERSION 260205"));
    CHECK(r.sources().slrf_sha256 == ODL_SLRF2020_SHA256);

    // the three unplaced SODs of pad 7307 (Ishigaki): A and C are absent from SLRF2020, B is listed with no solution
    const Epoch t = utc(1999, 6, 1, 0, 0, 0);
    for (long s : {73071701L, 73071702L, 73071703L}) {
        auto e = r.site(sod(s), t);
        INFO(s);
        REQUIRE_FALSE(e.has_value());
        CHECK(e.error().id == "MEAS-F-001");
        CHECK_THAT(e.error().message, ContainsSubstring("has no coordinates"));
    }
    auto b = r.site(sod(73071702), t);
    CHECK_THAT(b.error().message, ContainsSubstring("has no solution"));
    auto a = r.site(sod(73071701), t);
    CHECK_THAT(a.error().message, ContainsSubstring("does not"));                // "the eccentricity file lists it but SLRF2020 does not"
    // a SOD on one of the 49 pads SLRF2020 does not list (Ondrejov, 1148)
    auto ond = r.site(sod(11480901), t);
    REQUIRE_FALSE(ond.has_value());
    CHECK(ond.error().id == "MEAS-F-001");
    CHECK_THAT(ond.error().message, ContainsSubstring("has no coordinates"));
    CHECK_THAT(ond.error().message, ContainsSubstring("its pad is not listed either"));
    // Plan rule 3, the header's own "184 unique sites" against the 186 pads: counted here, by asking the registry
    int pads = 0, pads_without_the_two = 0;
    for (int p = 1; p <= 9999; ++p) {
        if (!r.knows_pad(PadId{p})) continue;
        ++pads;
        if (p != 7329 && p != 7317) ++pads_without_the_two;
    }
    CHECK(pads == 186);
    CHECK(r.knows_pad(PadId{7329}));                                             // Xian
    CHECK(r.knows_pad(PadId{7317}));                                             // Ishioka
    CHECK(pads_without_the_two == 184);                                          // 184 + 2 = 186
    const std::string text = slurp(ODL_SLRF2020_FILE);
    CHECK_THAT(text, ContainsSubstring("positions and velocities for 184 unique sites"));
    CHECK_THAT(text, ContainsSubstring("Xian (7329)"));                          // the FILE/COMMENT history adds both after that sentence
    CHECK_THAT(text, ContainsSubstring("Ishioka (7317)"));
    // an unknown pad refuses with the counts held
    auto pad = r.placed_sods_of(PadId{9999});
    REQUIRE_FALSE(pad.has_value());
    CHECK(pad.error().id == "MEAS-F-001");
    CHECK_THAT(pad.error().message, ContainsSubstring("186 pads"));
    auto yar = r.placed_sods_of(PadId{7090});
    REQUIRE(yar.has_value());
    CHECK(yar->size() == 13);                                                    // 70900501 … 70900513
}

TEST_CASE("MEAS-A-005  refusals on the real pinned data, each fired and shown not to fire on the adjacent input; a registry altered to "
          "return a nearest or default station fails the same assertions",
          "[measmod][registry][real]") {
    const SlrRegistry& r = real_registry();
    using Lookup = std::function<Result<SlrSite, MeasError>(SodKey, const Epoch&)>;
    const Lookup real = [&](SodKey k, const Epoch& t) { return r.site(k, t); };
    // the mutant: on ANY refusal it returns the first placed SOD of the same pad, or else Yarragadee's — the "nearest or default station"
    const Lookup naive = [&](SodKey k, const Epoch& t) -> Result<SlrSite, MeasError> {
        auto s = r.site(k, t);
        if (s.has_value()) return s;
        auto of_pad = r.placed_sods_of(PadId{k.pad});
        if (of_pad.has_value() && !of_pad->empty()) return r.site(of_pad->front(), t);
        return r.site(sod(70900513), t);
    };
    // the check: every case below must refuse with the stated id; it returns false at the first that does not
    struct Case { SodKey key; Epoch when; const char* id; };
    const std::vector<Case> cases = {
        {sod(99990101), utc(2020, 1, 1, 0, 0, 0), "MEAS-F-001"},                 // unknown SOD, unknown pad
        {sod(70900599), utc(2020, 1, 1, 0, 0, 0), "MEAS-F-001"},                 // a known pad, an unknown system
        {sod(70900513), utc(1992, 7, 20, 23, 59, 59), "MEAS-F-002"},              // one second before its first eccentricity span
        {sod(71100411), utc(1999, 10, 16, 20, 0, 0), "MEAS-F-003"},               // the 8-minute gap between solutions 1 and 2 of marker 7110 A
        {sod(73588901), utc(2026, 1, 1, 0, 0, 0), "MEAS-F-003"},                  // marker 7358 A's last solution ended 2019-12-14
    };
    auto all_refused = [&](const Lookup& f) {
        for (const auto& c : cases) {
            auto s = f(c.key, c.when);
            if (s.has_value() || s.error().id != std::string_view(c.id)) return false;
        }
        return true;
    };
    CHECK(all_refused(real));
    CHECK_FALSE(all_refused(naive));                                             // the check can fail: the mutant is caught
    // …and the adjacent inputs do NOT refuse: SOD 70900513 at its span's first instant; marker 7110 A at the end of solution 1 (inclusive)
    CHECK(r.site(sod(70900513), utc(1992, 7, 21, 0, 0, 0)).has_value());
    CHECK(r.site(sod(71100411), utc(1999, 10, 16, 15, 39, 36)).has_value());
    CHECK(r.site(sod(71100411), utc(1999, 10, 17, 0, 3, 58)).has_value());       // the start of solution 2
    // the unknown-SOD diagnostic names the number, the release, the hash and the counts held
    auto u = r.site(sod(99990101), utc(2020, 1, 1, 0, 0, 0));
    REQUIRE_FALSE(u.has_value());
    CHECK_THAT(u.error().message, ContainsSubstring("99990101"));
    CHECK_THAT(u.error().message, ContainsSubstring("VERSION 260205"));
    CHECK_THAT(u.error().message, ContainsSubstring(ODL_SLRF2020_SHA256));
    CHECK_THAT(u.error().message, ContainsSubstring("186 pads"));
    CHECK_THAT(u.error().message, ContainsSubstring("nearest-station"));
}

TEST_CASE("MEAS-A-010  real data: Yarragadee SOD 70900513 at the first normal point of the real January 2026 LAGEOS-1 file — the marker, the "
          "eccentricity, the system reference point and both geodetic coordinates against an independent evaluation",
          "[measmod][registry][real]") {
    const SlrRegistry& r = real_registry();
    const Epoch t = utc(2026, 1, 1, 2, 7, 56.8005871);                           // H4 start 2026-01-01 + record 11 seconds of day 7676.8005871
    auto s = r.site(sod(70900513), t);
    REQUIRE(s.has_value());
    CHECK_FALSE(s->post_seismic_override_used);
    CHECK(s->eccentricity_une_m.x == 3.1827);
    CHECK(s->eccentricity_une_m.y == -0.0064);
    CHECK(s->eccentricity_une_m.z == 0.0194);
    CHECK_THAT(s->marker_itrs_m.x, WithinAbs(ref::yarl_marker_x_m, 1e-6));
    CHECK_THAT(s->marker_itrs_m.y, WithinAbs(ref::yarl_marker_y_m, 1e-6));
    CHECK_THAT(s->marker_itrs_m.z, WithinAbs(ref::yarl_marker_z_m, 1e-6));
    CHECK_THAT(s->srp_itrs_m.x, WithinAbs(ref::yarl_srp_x_m, 1e-6));
    CHECK_THAT(s->srp_itrs_m.y, WithinAbs(ref::yarl_srp_y_m, 1e-6));
    CHECK_THAT(s->srp_itrs_m.z, WithinAbs(ref::yarl_srp_z_m, 1e-6));
    CHECK_THAT((s->srp_itrs_m - s->marker_itrs_m).norm(), WithinAbs(ref::yarl_srp_minus_marker_m, 1e-6));
    CHECK_THAT((s->srp_itrs_m - s->marker_itrs_m).norm(), WithinAbs(3.182766, 1e-6));   // |(3.1827, -0.0064, 0.0194)|
    CHECK_THAT(s->marker_geodetic.latitude_rad, WithinAbs(ref::yarl_marker_lat_rad, 1e-12));
    CHECK_THAT(s->marker_geodetic.longitude_rad, WithinAbs(ref::yarl_marker_lon_rad, 1e-12));
    CHECK_THAT(s->marker_geodetic.height_m, WithinAbs(ref::yarl_marker_h_m, 1e-6));
    CHECK_THAT(s->srp_geodetic.latitude_rad, WithinAbs(ref::yarl_srp_lat_rad, 1e-12));
    CHECK_THAT(s->srp_geodetic.longitude_rad, WithinAbs(ref::yarl_srp_lon_rad, 1e-12));
    CHECK_THAT(s->srp_geodetic.height_m, WithinAbs(ref::yarl_srp_h_m, 1e-6));
    // plausible by eye as well: -29.05 deg, 115.35 deg, 244 m
    CHECK_THAT(s->srp_geodetic.latitude_rad * 180.0 / kPi, WithinAbs(-29.0468, 5e-4));
    CHECK_THAT(s->srp_geodetic.longitude_rad * 180.0 / kPi, WithinAbs(115.3467, 5e-4));
    CHECK_THAT(s->srp_geodetic.height_m, WithinAbs(244.5, 0.1));
    CHECK(s->name == "Yarragadee MOBLAS-5");
    // the elapsed time the reference used: 11.000927789204093 years of 365.25 d; the position moves 5 cm/yr, so a 2 s error is 3e-9 m
    CHECK_THAT(t.difference(utc(2015, 1, 1, 0, 0, 0)).to_seconds() / (365.25 * 86400.0), WithinAbs(ref::yarl_elapsed_years, 1e-12));
}
