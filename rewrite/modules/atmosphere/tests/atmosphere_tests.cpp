// atmosphere_tests.cpp — L2 step 4's gate.
//
// SPEC-atmosphere.md §8.  THE GATE REPORTS ITS COUNTS, NOT ITS VERDICT.  NRL
// publishes no reference value for NRLMSISE-00 (§0), so what is compared here
// is the reference implementation's own output on its own published inputs —
// category 1 under plan §4 rule 6, not a last-ranked oracle comparison, and
// §8 states what that cannot check.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/atmosphere/atmosphere.hpp>

#include "msis_model.hpp"
#include "msis_reference_values.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace odl::atmosphere;
using namespace odl::atmosphere::detail;
namespace ref = odl::atmosphere::reference;

namespace {

struct Case { double iyd, sec, alt, lat, lon, stl, f107a, f107, ap; bool hourly; };

/// The 17 published input cases, transcribed from NRLMSISE-00.FOR lines
/// 2438-2552's DATA statements.  15 from the DO loop + 2 with the seven-element
/// Ap and SW(9) = -1.  These are PUBLISHED INPUTS; there are no published
/// outputs to go with them, which is the whole of §0's finding.
std::vector<Case> published_cases() {
    const double IDAY[15] = {172,81,172,172,172,172,172,172,172,172,172,172,172,172,172};
    const double UT[15]   = {29000,29000,75000,29000,29000,29000,29000,29000,
                             29000,29000,29000,29000,29000,29000,29000};
    const double ALT[15]  = {400,400,1000,100,400,400,400,400,400,400,0,10,30,50,70};
    const double LAT[15]  = {60,60,60,60,0,60,60,60,60,60,60,60,60,60,60};
    const double LON[15]  = {-70,-70,-70,-70,-70,0,-70,-70,-70,-70,-70,-70,-70,-70,-70};
    const double LST[15]  = {16,16,16,16,16,16,4,16,16,16,16,16,16,16,16};
    const double FA[15]   = {150,150,150,150,150,150,150,70,150,150,150,150,150,150,150};
    const double FD[15]   = {150,150,150,150,150,150,150,150,180,150,150,150,150,150,150};
    const double AP[15]   = {4,4,4,4,4,4,4,4,4,40,4,4,4,4,4};
    std::vector<Case> v;
    for (int i = 0; i < 15; ++i)
        v.push_back({IDAY[i],UT[i],ALT[i],LAT[i],LON[i],LST[i],FA[i],FD[i],AP[i],false});
    v.push_back({IDAY[0],UT[0],ALT[0],LAT[0],LON[0],LST[0],FA[0],FD[0],100,true});
    v.push_back({IDAY[0],UT[0],ALT[3],LAT[0],LON[0],LST[0],FA[0],FD[0],100,true});
    return v;
}

/// The sweep, identical to tools/msis_reference.py's.  It crosses every branch
/// boundary the model has; a sweep that never crosses one cannot fail on one.
/// The last 14 points are close pairs straddling each of the seven species-
/// correction cutoffs NRLMSISE-00.FOR's own DATA ALTL sets above 120 km (N2
/// 160, He 200, Ar 240, O2 250, O 300, H 320, N 450 km) -- added when L4 step
/// 4's own drag Jacobian found the 300 km one was a real, reference-level
/// discontinuity and a direct read of the pinned source found the other six
/// (SPEC-atmosphere §3.1, PROVENANCE.md §28.5/§28.10).
const double kSweepAlt[] = {0,5,10,15,20,25,32.5,40,45,55,62.5,70,72.5,80,90,100,110,120,
                            150,200,300,400,550,700,1000,1500,2000,
                            159.99,160.01, 199.99,200.01, 239.99,240.01, 249.99,250.01,
                            299.99,300.01, 319.99,320.01, 449.99,450.01};
struct Cond { double iyd, sec, lat, lon, fa, fd, ap; };
const Cond kSweepCond[] = {{172,29000,60,-70,150,150,4}, {81,29000,0,0,70,70,0},
                           {355,75000,-80,170,250,300,200}, {200,43200,45,90,100,90,15}};

std::vector<Case> all_cases() {
    std::vector<Case> v = published_cases();
    for (const auto& c : kSweepCond)
        for (double a : kSweepAlt)
            v.push_back({c.iyd,c.sec,a,c.lat,c.lon,c.sec/3600.0+c.lon/15.0,c.fa,c.fd,c.ap,false});
    return v;
}

constexpr double kAmu = 1.66e-24;
constexpr double kMass[9] = {4,16,28,32,40,0,1,14,16};

RawResult run7(const Case& c) {
    DailyAp d{c.ap}; ThreeHourlyAp h{}; h.ap.fill(c.ap);
    return gtd7(c.iyd,c.sec,c.alt,c.lat,c.lon,c.stl,c.f107a,c.f107,d,h,c.hourly);
}
RawResult run7d(const Case& c) {
    DailyAp d{c.ap}; ThreeHourlyAp h{}; h.ap.fill(c.ap);
    return gtd7d(c.iyd,c.sec,c.alt,c.lat,c.lon,c.stl,c.f107a,c.f107,d,h,c.hourly);
}

}  // namespace

TEST_CASE("ATMO-A-001  the port against the reference, with its five counts", "[atmosphere][gate]") {
    const auto cases = all_cases();

    // count 1 and 2: the cases actually run, against their declared sizes.  A
    // count of passed cases with no denominator is what hid 868 ephemeris cases
    // at step 1 (plan §4 rule 3).
    REQUIRE(published_cases().size() == ref::kPublishedCases);
    REQUIRE(cases.size() - published_cases().size() == ref::kSweepCases);
    REQUIRE(cases.size() == ref::kValues.size());

    std::size_t material = 0, immaterial = 0, zero = 0, quantities = 0;
    double worst = 0.0;
    std::size_t worst_case = 0; int worst_q = -1;

    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto r7 = run7(cases[i]);
        const auto r7d = run7d(cases[i]);
        REQUIRE(r7.fault.ok());
        REQUIRE(r7d.fault.ok());

        double mine[12];
        for (std::size_t j = 0; j < 9; ++j) mine[j] = r7.d[j];
        mine[9] = r7.t[0]; mine[10] = r7.t[1]; mine[11] = r7d.d[5];

        const auto& R = ref::kValues[i];
        double theirs[12];
        for (std::size_t j = 0; j < 11; ++j) theirs[j] = R.gtd7[j];
        theirs[11] = R.gtd7d_rho;

        // count 3: quantities per case
        for (int j = 0; j < 12; ++j) {
            ++quantities;
            if (theirs[j] == 0.0) {
                // the documented zeros: a correct port produces them exactly
                REQUIRE(mine[j] == 0.0);
                ++zero; continue;
            }
            const double frac = (j < 9 && j != 5) ? kMass[j] * theirs[j] * kAmu / theirs[5] : 1.0;
            const double rel = std::abs(mine[j] - theirs[j]) / std::abs(theirs[j]);
            if (frac < ref::kMaterialFraction) { ++immaterial; continue; }
            ++material;
            if (rel > worst) { worst = rel; worst_case = i; worst_q = j; }
        }
    }

    INFO("worst material disagreement " << worst << " at case " << worst_case
         << " quantity " << worst_q);
    CHECK(quantities == cases.size() * 12);
    CHECK(material == ref::kClassMaterial);
    CHECK(zero == ref::kClassZero);
    // class B here absorbs the reference's class C: those 18 comparisons have a
    // non-zero promoted-double value, so they are immaterial rather than zero.
    CHECK(immaterial == ref::kClassImmaterial + ref::kClassUnderflowed);

    // ATMO-P-3: the port is DOUBLE precision and the reference's frozen values are
    // its promoted-double build, so this is roundoff, not the 1e-5 that bounds
    // the single-precision build.
    CHECK(worst < 1.0e-12);
}

// odl maintainer's item 4: promote the drag Jacobian's own diagnostic (which
// found the 300 km cutoff by measurement, not by reading the source) to a
// gated test HERE, in this module's own suite, against FROZEN reference
// output -- not the live reference binary, which CI must never need
// (ATMO-Q-005). ATMO-A-001's general sweep already covers these 14 points
// (they are 14 of its 181), passing at the same <1e-12 bound; this test
// names each of the seven cutoffs explicitly rather than leaving their
// coverage to be inferred from a count, and additionally confirms the
// discontinuity is really THERE in the frozen values, not only that the
// port tracks a smooth reference closely.
TEST_CASE("ATMO-A-028  the seven species-correction cutoffs above 120 km, port against "
          "frozen reference on both sides of each, and each cutoff genuinely jumps",
          "[atmosphere][gate]") {
    const auto cases = all_cases();
    REQUIRE(cases.size() == ref::kValues.size());
    const std::size_t published = published_cases().size();
    constexpr std::size_t kAltCount = sizeof(kSweepAlt) / sizeof(kSweepAlt[0]);
    constexpr std::size_t kFirstCutoffAltIdx = kAltCount - 14;   // the 14 appended last

    struct Cutoff { const char* species; double alt_km; };
    constexpr Cutoff kCutoffs[7] = {
        {"N2", 160.0}, {"He", 200.0}, {"Ar", 240.0}, {"O2", 250.0},
        {"O", 300.0}, {"H", 320.0}, {"N", 450.0}};

    // Condition 0 (the driver's own baseline) throughout: rule 3, one place,
    // one reference point, not one cutoff examined closely and six assumed
    // similar (the same discipline the jump table in PROVENANCE.md §28.10
    // applies, and the same reason it measured all seven rather than one).
    for (std::size_t c = 0; c < 7; ++c) {
        const std::size_t below_alt_idx = kFirstCutoffAltIdx + 2 * c;       // e.g. 159.99
        const std::size_t above_alt_idx = kFirstCutoffAltIdx + 2 * c + 1;   // e.g. 160.01
        const std::size_t below_idx = published + below_alt_idx;   // condition 0: no offset
        const std::size_t above_idx = published + above_alt_idx;

        // Sanity: the index arithmetic actually lands on the altitude it
        // claims to, checked before trusting the paired reference record --
        // Record itself carries no input fields to check this against
        // directly (by design, ATMO-R-028's own minimal shape), so this is
        // checked against this file's OWN case list instead.
        REQUIRE(cases[below_idx].alt == kSweepAlt[below_alt_idx]);
        REQUIRE(cases[above_idx].alt == kSweepAlt[above_alt_idx]);
        REQUIRE_THAT(kSweepAlt[below_alt_idx],
                    Catch::Matchers::WithinAbs(kCutoffs[c].alt_km - 0.01, 1.0e-9));
        REQUIRE_THAT(kSweepAlt[above_alt_idx],
                    Catch::Matchers::WithinAbs(kCutoffs[c].alt_km + 0.01, 1.0e-9));

        const auto below_port = run7d(cases[below_idx]);
        const auto above_port = run7d(cases[above_idx]);
        REQUIRE(below_port.fault.ok());
        REQUIRE(above_port.fault.ok());
        const double below_ref = ref::kValues[below_idx].gtd7d_rho;
        const double above_ref = ref::kValues[above_idx].gtd7d_rho;

        const double rel_below = std::abs(below_port.d[5] - below_ref) / std::abs(below_ref);
        const double rel_above = std::abs(above_port.d[5] - above_ref) / std::abs(above_ref);
        const double ref_jump = std::abs(above_ref - below_ref) / std::abs(below_ref);
        INFO(kCutoffs[c].species << " @ " << kCutoffs[c].alt_km << " km: port/ref below="
             << below_port.d[5] << "/" << below_ref << " (rel " << rel_below
             << "), above=" << above_port.d[5] << "/" << above_ref << " (rel " << rel_above
             << "); reference's own relative jump = " << ref_jump);

        // port matches the frozen reference at both sides, to the same
        // double-precision-roundoff bound ATMO-A-001 asserts generally
        CHECK(rel_below < 1.0e-12);
        CHECK(rel_above < 1.0e-12);
        // and the reference itself genuinely jumps here -- 1e-6 is three
        // orders below the smallest of the seven measured jumps (N @ 450 km,
        // ~5e-5, PROVENANCE.md §28.10), comfortable margin without chasing
        // the exact per-cutoff figure a future reference re-pin might move
        // slightly.
        CHECK(ref_jump > 1.0e-6);
    }
}

TEST_CASE("ATMO-A-004  total density against the species sum, not through the reference",
          "[atmosphere]") {
    double worst7 = 0.0, worst7d = 0.0;
    std::size_t n = 0;
    for (const auto& c : all_cases()) {
        const auto r = run7(c);
        const auto rd = run7d(c);
        const double sum = kAmu * (4*r.d[0] + 16*r.d[1] + 28*r.d[2] + 32*r.d[3]
                                 + 40*r.d[4] + r.d[6] + 14*r.d[7]);
        worst7 = std::max(worst7, std::abs(sum - r.d[5]) / std::abs(r.d[5]));
        const double sumd = sum + 16.0 * kAmu * r.d[8];
        worst7d = std::max(worst7d, std::abs(sumd - rd.d[5]) / std::abs(rd.d[5]));
        ++n;
    }
    INFO("cases " << n << " worst GTD7 " << worst7 << " worst GTD7D " << worst7d);
    REQUIRE(n == ref::kValues.size());
    CHECK(worst7 < 1.0e-15);
    CHECK(worst7d < 1.0e-15);
}

TEST_CASE("ATMO-A-005  O, H, N and anomalous O are exactly zero below 72.5 km", "[atmosphere]") {
    std::size_t checked = 0;
    for (const auto& c : all_cases()) {
        if (c.alt >= 72.5) continue;
        const auto r = run7(c);
        for (std::size_t j : {1u, 6u, 7u, 8u}) { CHECK(r.d[j] == 0.0); ++checked; }
    }
    INFO("values checked below 72.5 km: " << checked);
    CHECK(checked > 0);
}

TEST_CASE("ATMO-A-006  GTD7D exceeds GTD7 by exactly the anomalous-oxygen mass", "[atmosphere]") {
    double worst = 0.0;
    for (const auto& c : all_cases()) {
        const auto r = run7(c);
        const auto rd = run7d(c);
        const double expected = r.d[5] + 16.0 * kAmu * r.d[8];
        if (rd.d[5] != 0.0) worst = std::max(worst, std::abs(expected - rd.d[5]) / rd.d[5]);
    }
    INFO("worst " << worst);
    CHECK(worst < 1.0e-15);
}

TEST_CASE("ATMO-A-027  an unresolved species is a refusal, not a zero", "[atmosphere]") {
    // 100 km: the reference underflows anomalous oxygen to exactly zero here.
    const Case low{172, 29000, 100, 60, -70, 16, 150, 150, 4, false};
    const auto r = run7(low);
    const double rho = r.d[5];
    SpeciesDensity anom{r.d[8] * 1e6, 16.0 * r.d[8] * kAmu * 1e3,
                        16.0 * r.d[8] * kAmu / rho >= 1e-15};
    REQUIRE_FALSE(anom.resolved());
    const auto n = anom.number_density_m3();
    REQUIRE_FALSE(n.has_value());
    CHECK(n.error().id == "ATMO-F-017");
    // and the drag question is answered without a refusal
    CHECK(anom.mass_density_kg_m3() >= 0.0);

    // PROVEN BOTH WAYS: a resolved species does not refuse.
    const Case high{172, 29000, 400, 60, -70, 16, 150, 150, 4, false};
    const auto r2 = run7(high);
    SpeciesDensity o{r2.d[1] * 1e6, 16.0 * r2.d[1] * kAmu * 1e3,
                     16.0 * r2.d[1] * kAmu / r2.d[5] >= 1e-15};
    REQUIRE(o.resolved());
    CHECK(o.number_density_m3().has_value());
}

TEST_CASE("ATMO-A-017  the model discards the year", "[atmosphere]") {
    const Case a{172, 29000, 400, 60, -70, 16, 150, 150, 4, false};
    const Case b{99172, 29000, 400, 60, -70, 16, 150, 150, 4, false};
    CHECK(run7(a).d[5] == run7(b).d[5]);
}

TEST_CASE("ATMO-A-024  two snapshots cannot be compared silently", "[atmosphere]") {
    DragDensity a{}, b{};
    a.total_mass_kg_m3 = 1.0e-12; b.total_mass_kg_m3 = 2.0e-12;
    a.record.snapshot_id = "gfz-kp-ap-f107"; a.record.snapshot_sha256 = "0121821b2079973100";
    b.record = a.record;

    const auto same = density_ratio(a, b);
    REQUIRE(same.has_value());
    CHECK_THAT(*same, Catch::Matchers::WithinRel(0.5, 1e-15));

    b.record.snapshot_sha256 = "deadbeefdeadbeef00";
    const auto crossed = density_ratio(a, b);
    REQUIRE_FALSE(crossed.has_value());
    CHECK(crossed.error().id == "ATMO-F-013");
}
