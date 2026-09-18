// tides_tests.cpp — SPEC-perturbations §§4.3–4.4 and §8.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/tides/pole.hpp>
#include <odl/tides/solid_earth.hpp>

#include "solid_tide_tables.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>
#include <type_traits>

using namespace odl;
using namespace odl::tides;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr double kArcsec = std::numbers::pi / (180.0 * 3600.0);

const OceanPoleTide& ocean() {
    static const auto o = OceanPoleTide::load(ODL_DESAI_POLE_COEF, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(o.has_value());
    return *o;
}

/// A wobble of (m1, m2) in arcseconds, built against a zero secular pole so
/// that the test controls the wobble directly.  PERT-A-009 exercises the real
/// route, where the secular pole is gravity's.
Wobble wobble_arcsec(double m1, double m2) {
    return Wobble::from(m1 * kArcsec, -m2 * kArcsec, gravity::PoleCoordinates{0.0, 0.0});
}
}  // namespace

TEST_CASE("PERT-A-005: the solid Earth pole tide, from its constants", "[tides]") {
    // The leading coefficient, per second of arc, reproduced from k2 and TN36-1.
    const double per_arcsec = -SolidEarthPoleTide::leading_coefficient_per_radian() * kArcsec;
    WARN("PERT-A-005: leading coefficient " << per_arcsec << " per arcsec, against TN36-6 §6.4's "
         "printed " << SolidEarthPoleTide::kPrintedLeadingPerArcsec);
    CHECK_THAT(per_arcsec, WithinRel(-1.333237e-9, 1e-6));
    CHECK_THAT(per_arcsec, WithinRel(SolidEarthPoleTide::kPrintedLeadingPerArcsec, 1e-3));

    // AND THE DISAGREEMENT INSIDE §6.4, REPORTED RATHER THAN RECONCILED.
    const double from_k2 = SolidEarthPoleTide::kK2Imag / SolidEarthPoleTide::kK2Real;
    const double printed = SolidEarthPoleTide::kPrintedCrossTermRatio;
    const double implied_k2_imag = printed * SolidEarthPoleTide::kK2Real;
    WARN("PERT-A-005: TN36-6 §6.4's cross-term ratio is printed as " << printed
         << " where Im(k2)/Re(k2) = " << from_k2 << "; the printed ratio implies Im(k2) = "
         << implied_k2_imag << " against the printed " << SolidEarthPoleTide::kK2Imag
         << ". This module follows k2. The difference is "
         << std::abs(from_k2 - printed) * 100.0 << "% of the m2 term.");
    CHECK_THAT(from_k2, WithinRel(0.011700, 1e-4));
    CHECK(std::abs(from_k2 - printed) / from_k2 > 0.01);     // they really do differ
    CHECK(std::abs(from_k2 - printed) / from_k2 < 0.03);     // and only by that much

    // The printed form, term by term, at m1 = 1" and at m2 = 1".
    auto only_m1 = SolidEarthPoleTide::increments(wobble_arcsec(1.0, 0.0));
    auto only_m2 = SolidEarthPoleTide::increments(wobble_arcsec(0.0, 1.0));
    REQUIRE(only_m1.has_value());
    REQUIRE(only_m2.has_value());
    CHECK_THAT(only_m1->dc(2, 1), WithinRel(-1.333237e-9, 1e-6));
    CHECK_THAT(only_m2->dc(2, 1), WithinRel(-1.333237e-9 * from_k2, 1e-6));
    CHECK_THAT(only_m2->ds(2, 1), WithinRel(-1.333237e-9, 1e-6));
    CHECK_THAT(only_m1->ds(2, 1), WithinRel(1.333237e-9 * from_k2, 1e-6));
    // nothing outside (2,1)
    CHECK(only_m1->dc(2, 0) == 0.0);
    CHECK(only_m1->dc(2, 2) == 0.0);
}

TEST_CASE("PERT-A-006: the ocean pole tide reproduces TN36-6 (6.24)", "[tides]") {
    const OceanPoleTide& o = ocean();
    WARN("PERT-A-006: " << o.rows() << " rows to degree " << o.file_max_degree()
         << "; R_2 = " << o.r_n(2));
    CHECK(o.file_max_degree() == 360);

    auto m1 = o.increments(wobble_arcsec(1.0, 0.0), 2);
    auto m2 = o.increments(wobble_arcsec(0.0, 1.0), 2);
    REQUIRE(m1.has_value());
    REQUIRE(m2.has_value());

    // TN36-6 (6.24), per ARCSECOND:
    //   dC21 = -2.1778e-10 (m1 - 0.01724 m2)
    //   dS21 = -1.7232e-10 (m2 - 0.03365 m1)
    WARN("PERT-A-006: dC21 = " << m1->dc(2, 1) << " m1 + " << m2->dc(2, 1)
         << " m2 ; dS21 = " << m1->ds(2, 1) << " m1 + " << m2->ds(2, 1) << " m2  (per arcsec)");
    CHECK_THAT(m1->dc(2, 1), WithinRel(-2.1778e-10, 5e-5));
    CHECK_THAT(m2->ds(2, 1), WithinRel(-1.7232e-10, 5e-5));
    CHECK_THAT(m2->dc(2, 1) / m1->dc(2, 1), WithinRel(-0.01724, 1e-3));
    CHECK_THAT(m1->ds(2, 1) / m2->ds(2, 1), WithinRel(-0.03365, 1e-3));

    // PERT-R-033, and a statistic that carries its formula.  TN36-6 §6.5:
    // "Approximately 90% of the variance of the ocean pole tide POTENTIAL is
    // provided by the degree n = 2 spherical harmonic components ... Expansion
    // to spherical harmonic degree n = 10 provides approximately 99%."  That is
    // the R_n-weighted variance; the raw coefficient variance gives 75.8% and
    // 92.7% at the same degrees, and reporting it under the same word would
    // have been a statistic without its formula.
    for (int d : {2, 3, 10, 50, 100, 360}) {
        WARN("PERT-A-006: degree " << d << " retains " << o.potential_variance_fraction(d) * 100.0
             << "% of the POTENTIAL variance and " << o.coefficient_variance_fraction(d) * 100.0
             << "% of the raw COEFFICIENT variance");
    }
    CHECK_THAT(o.potential_variance_fraction(2), WithinAbs(0.90, 0.015));
    CHECK_THAT(o.potential_variance_fraction(10), WithinAbs(0.99, 0.01));
    CHECK(o.coefficient_variance_fraction(2) < 0.80);      // the other statistic, and it differs
    CHECK(o.potential_variance_fraction(360) == 1.0);

    // PERT-R-034: degree 1 is not exported.
    auto full = o.increments(wobble_arcsec(0.3, 0.2), 10);
    REQUIRE(full.has_value());
    CHECK(full->dc(1, 0) == 0.0);
    CHECK(full->dc(1, 1) == 0.0);
    CHECK(full->ds(1, 1) == 0.0);
}

TEST_CASE("PERT-A-008: the sign on m2 is a phase, not an amplitude", "[tides]") {
    // TN36-7 (25) is m2 = -(yp - ys).  Getting the sign wrong leaves the
    // amplitude of the pole tide unchanged and inverts its phase, so the test
    // compares a phase.
    const gravity::PoleCoordinates secular{0.0, 0.0};
    const double xp = 0.15 * kArcsec, yp = 0.35 * kArcsec;
    const Wobble right = Wobble::from(xp, yp, secular);
    CHECK_THAT(right.m1_arcsec(), WithinRel(0.15, 1e-12));
    CHECK_THAT(right.m2_arcsec(), WithinRel(-0.35, 1e-12));

    auto a = SolidEarthPoleTide::increments(right);
    REQUIRE(a.has_value());
    // the wrong sign, as a caller who read (25) as m2 = +(yp - ys) would have it
    auto b = SolidEarthPoleTide::increments(Wobble::from(xp, -yp, secular));
    REQUIRE(b.has_value());
    const double amp_a = std::hypot(a->dc(2, 1), a->ds(2, 1));
    const double amp_b = std::hypot(b->dc(2, 1), b->ds(2, 1));
    const double phase_a = std::atan2(a->ds(2, 1), a->dc(2, 1));
    const double phase_b = std::atan2(b->ds(2, 1), b->dc(2, 1));
    WARN("PERT-A-008: amplitudes " << amp_a << " and " << amp_b << " (ratio " << amp_b / amp_a
         << "); phases " << phase_a << " and " << phase_b << " rad");
    CHECK_THAT(amp_b, WithinRel(amp_a, 1e-12));          // indistinguishable by amplitude
    CHECK(std::abs(phase_a - phase_b) > 0.5);            // but not by phase
}

TEST_CASE("PERT-A-009: the secular pole is gravity's, consumed not restated", "[tides]") {
    // Moving gravity's definition moves this module's pole tide, which is what
    // "consumes that definition" has to mean if it is to be checkable.
    const double xp = 0.15 * kArcsec, yp = 0.35 * kArcsec;
    const auto at_2000 = gravity::SecularPole::at_years(0.0);
    const auto at_2026 = gravity::SecularPole::at_years(26.0);
    CHECK(at_2000.x_rad != at_2026.x_rad);

    auto a = SolidEarthPoleTide::increments(Wobble::from(xp, yp, at_2000));
    auto b = SolidEarthPoleTide::increments(Wobble::from(xp, yp, at_2026));
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    const double moved = std::abs(b->dc(2, 1) - a->dc(2, 1));
    WARN("PERT-A-009: 26 years of secular pole drift moves dC21 by " << moved
         << ", which is " << moved / std::abs(a->dc(2, 1)) * 100.0 << "% of it");
    CHECK(moved > 0.0);
    CHECK(moved / std::abs(a->dc(2, 1)) > 0.05);
}

TEST_CASE("PERT-F-001 / F-005 / F-010: refusals", "[tides]") {
    auto outside = OceanPoleTide::load("/etc/hostname", ODL_MANIFEST_CACHE_ROOT);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().id == "PERT-F-010");

    auto too_far = ocean().increments(wobble_arcsec(0.1, 0.1), 400);
    REQUIRE_FALSE(too_far.has_value());
    CHECK(too_far.error().id == "PERT-F-005");
    CHECK(too_far.error().message.find("360") != std::string::npos);

    // A tide-free increment cannot be summed with a zero-tide one.
    auto zero_tide = SolidEarthPoleTide::increments(wobble_arcsec(0.1, 0.1));
    REQUIRE(zero_tide.has_value());
    TideIncrements tide_free{2, gravity::TideSystem::TideFree,
                             ModelRecord{"a synthetic tide-free model", "this test", ""}};
    tide_free.add(2, 0, 1e-11, 0.0);
    auto bad = TideIncrements::sum({&*zero_tide, &tide_free});
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "PERT-F-001");
    CHECK(bad.error().message.find("6.13") != std::string::npos);

    // and the same systems do sum
    auto good = TideIncrements::sum({&*zero_tide, &*zero_tide});
    REQUIRE(good.has_value());
    CHECK_THAT(good->dc(2, 1), WithinRel(2.0 * zero_tide->dc(2, 1), 1e-15));
    CHECK(good->models().size() == 2);

    static_assert(!std::is_default_constructible_v<Wobble>);
    static_assert(!std::is_default_constructible_v<OceanPoleTide>);
}

// ---------------------------------------------------------------------------
// Solid Earth tides — TN36-6 §6.2
// ---------------------------------------------------------------------------

#include <algorithm>
#include <complex>

namespace {
/// The printed amplitudes, re-read here from the generated tables so that the
/// test compares against the table rather than against the module's copy of it.
/// PERT-A-001 can fail on the WIRING — a swapped column, a wrong eta_m, a sign,
/// a dropped constituent — and cannot fail on a wrong amplitude, because the
/// amplitude is what the module was given. §8 says so.
struct Printed { double ip, op, dk_r, dk_i, deg_per_hour; };

Printed printed_row(int band, std::size_t i) {
    using namespace odl::tides::tables;
    const SolidTideTerm* rows = band == 0 ? kSolidTideZonal
                              : band == 1 ? kSolidTideDiurnal
                                          : kSolidTideSemidiurnal;
    const SolidTideTerm& r = rows[i];
    return Printed{r.amp_ip, r.amp_op, r.dk_real, r.dk_imag, r.deg_per_hour};
}
}  // namespace

TEST_CASE("PERT-A-001: Step 2 at theta_f = 0, term by term, with both counts", "[tides]") {
    const double s = 1e-12;
    int checked = 0;
    double worst = 0.0;

    // Band 1, Table 6.5a: eta_1 = -i, so dC21 = Amp(op) and dS21 = Amp(ip).
    for (std::size_t i = 0; i < SolidEarthTide::constituents(1); ++i) {
        auto r = SolidEarthTide::step2_one(1, i, 0.0);
        REQUIRE(r.has_value());
        const Printed p = printed_row(1, i);
        INFO("Table 6.5a row " << i << " at " << p.deg_per_hour << " deg/hr");
        CHECK_THAT(r->dc(2, 1), WithinAbs(p.op * s, 1e-25));
        CHECK_THAT(r->ds(2, 1), WithinAbs(p.ip * s, 1e-25));
        worst = std::max({worst, std::abs(r->dc(2, 1) - p.op * s),
                          std::abs(r->ds(2, 1) - p.ip * s)});
        ++checked;
    }
    // Band 2, Table 6.5c: eta_2 = 1, so dC22 = Amp(ip) and dS22 = -Amp(op).
    for (std::size_t i = 0; i < SolidEarthTide::constituents(2); ++i) {
        auto r = SolidEarthTide::step2_one(2, i, 0.0);
        REQUIRE(r.has_value());
        const Printed p = printed_row(2, i);
        INFO("Table 6.5c row " << i);
        CHECK_THAT(r->dc(2, 2), WithinAbs(p.ip * s, 1e-25));
        CHECK_THAT(r->ds(2, 2), WithinAbs(-p.op * s, 1e-25));
        ++checked;
    }

    // THE TWO COUNTS.  A count of passed terms without its denominator is what
    // hid 868 ephemeris cases at step 1.
    WARN("PERT-A-001: " << checked << " constituents checked against their printed amplitudes ("
         << SolidEarthTide::constituents(1) << " diurnal + " << SolidEarthTide::constituents(2)
         << " semidiurnal), worst residual " << worst << ".\n"
         "    The Conventions print NO expected value for: step 1's time-domain evaluation at "
         "any epoch; the ocean tide sum at any epoch; the ocean pole tide above degree 2; the "
         "relativistic correction as a vector; third-body attraction, which they do not treat; "
         "and step 2's amplitudes as anything other than their own input, which is what this "
         "test compares against. An independent H_f catalogue is the one thing that would move "
         "the last item off that list (PERT-Q-004).");
    CHECK(checked == static_cast<int>(SolidEarthTide::constituents(1)
                                      + SolidEarthTide::constituents(2)));
}

TEST_CASE("PERT-A-003: Table 6.5b, the zonal band, through (6.8a)", "[tides]") {
    int checked = 0;
    for (std::size_t i = 0; i < SolidEarthTide::constituents(0); ++i) {
        auto r = SolidEarthTide::step2_one(0, i, 0.0);
        REQUIRE(r.has_value());
        const Printed p = printed_row(0, i);
        INFO("Table 6.5b row " << i << " at " << p.deg_per_hour << " deg/hr");
        CHECK_THAT(r->dc(2, 0), WithinAbs(p.ip * 1e-12, 1e-25));
        CHECK(r->ds(2, 0) == 0.0);          // S_n0 is zero by definition
        ++checked;
    }
    WARN("PERT-A-003: " << checked << " zonal constituents against their printed amplitudes");
    CHECK(checked == 21);
}

TEST_CASE("PERT-A-002: the tables' own internal relation, with its rounding bound", "[tides]") {
    // Amp(ip) * dkI == Amp(op) * dkR, because both amplitudes are the same
    // A_m H_f times their own part of dk.  It needs no H_f and it is the check
    // that the columns were read in the right order.  The amplitudes are printed
    // to 0.1e-12, so the bound is 0.05 (|dkI| + |dkR|); a row where the printed
    // precision swamps both products CONSTRAINS NOTHING and is counted, not
    // passed.
    for (int band : {0, 1}) {
        int applicable = 0, constraining = 0;
        double worst = 0.0;
        for (std::size_t i = 0; i < SolidEarthTide::constituents(band); ++i) {
            const Printed p = printed_row(band, i);
            if (p.dk_i == 0.0 && p.op == 0.0) continue;
            ++applicable;
            const double residual = p.ip * p.dk_i - p.op * p.dk_r;
            const double bound = 0.05 * (std::abs(p.dk_i) + std::abs(p.dk_r));
            const double terms = std::max(std::abs(p.ip * p.dk_i), std::abs(p.op * p.dk_r));
            if (terms <= bound) continue;
            ++constraining;
            INFO("band " << band << " row " << i << " at " << p.deg_per_hour << " deg/hr");
            CHECK(std::abs(residual) <= bound);
            worst = std::max(worst, std::abs(residual) / bound);
        }
        WARN("PERT-A-002: band " << band << " — " << constraining << " of " << applicable
             << " rows constrain the relation (" << (applicable - constraining)
             << " lost to the printed precision), worst residual " << worst
             << " of its rounding bound");
        CHECK(constraining > 10);
    }
}

TEST_CASE("PERT-A-025: the dk column's RESONANCE STRUCTURE against (6.9)", "[tides]") {
    // WHAT (6.9) CAN AND CANNOT CHECK, because the specification asked for more
    // than it can give.  TN36-6's definition below (6.8e) is explicit:
    //
    //   dk_f = [ k_f at frequency f  -  the nominal k21 ]  PLUS A CONTRIBUTION
    //          FROM OCEAN LOADING
    //
    // and (6.9) with Table 6.4's k21 parameters produces only the first part.
    // §6.2.1 says the load-resonance corrections are "incorporated through
    // equivalent corrections to the body tide Love numbers ... also included in
    // the tables".  So the formula cannot reproduce the tabulated dk, and a test
    // that demanded it would be comparing a partial quantity with a complete one.
    //
    // What it CAN check is the resonance structure, and that is worth having:
    // the ratio of the printed dk to the formula's must be CONSTANT across the
    // band, because the loading contribution shares the same resonance
    // denominators.  Across a band where dk itself varies by a factor of about
    // 3000, a constant ratio is a strong statement about sigma_alpha and L_alpha.
    // The negative control at the end shows what a wrong resonance frequency
    // does to it.
    using C = std::complex<double>;
    const C L0{0.29954, -0.1412e-2};
    const std::array<C, 3> L{C{-0.77896e-3, -0.3711e-4}, C{0.90963e-4, -0.2963e-5},
                             C{-0.11416e-5, 0.5325e-7}};
    const std::array<C, 3> sigma{C{-0.0026010, -0.0001361}, C{1.0023181, 0.000025},
                                 C{0.999026, 0.000780}};
    const C k21_nominal{0.29830, -0.00144};

    auto formula = [&](double deg_per_hour, const std::array<C, 3>& sig) {
        const double s = deg_per_hour / (15.0 * 1.002737909);
        C Lsig = L0;
        for (std::size_t a = 0; a < 3; ++a) Lsig += L[a] / (C{s, 0.0} - sig[a]);
        return (Lsig - k21_nominal) * 1e5;
    };

    auto spread_of = [&](const std::array<C, 3>& sig, int& used, double& lo, double& hi,
                         double& span) {
        std::vector<double> ratio;
        double min_dk = 1e300, max_dk = 0.0;
        for (std::size_t i = 0; i < SolidEarthTide::constituents(1); ++i) {
            const Printed p = printed_row(1, i);
            const C f = formula(p.deg_per_hour, sig);
            min_dk = std::min(min_dk, std::abs(p.dk_r));
            max_dk = std::max(max_dk, std::abs(p.dk_r));
            if (std::abs(f.real()) < 1.0) continue;   // a zero crossing constrains nothing
            ratio.push_back(p.dk_r / f.real());
        }
        used = static_cast<int>(ratio.size());
        lo = *std::min_element(ratio.begin(), ratio.end());
        hi = *std::max_element(ratio.begin(), ratio.end());
        std::sort(ratio.begin(), ratio.end());
        span = max_dk / min_dk;
        return ratio[ratio.size() / 2];
    };

    int used = 0;
    double lo = 0.0, hi = 0.0, span = 0.0;
    const double median = spread_of(sigma, used, lo, hi, span);
    const double relative_spread = (hi - lo) / median;
    WARN("PERT-A-025: " << used << " of " << SolidEarthTide::constituents(1)
         << " diurnal constituents; printed dk spans a factor of " << span
         << " across the resonance, and printed/formula on the REAL part is " << median
         << " with a spread of " << relative_spread * 100.0 << "%.\n"
         "    The formula does not reproduce dk and cannot: TN36-6 defines dk_f as the body-tide "
         "difference PLUS A CONTRIBUTION FROM OCEAN LOADING, and (6.9) gives only the first. The "
         "near-constant offset of " << (median - 1.0) * 100.0 << "% IS that contribution, which "
         "shares the same resonance denominators. What this checks is the resonance structure.");
    CHECK(used > 40);
    CHECK(span > 1000.0);
    CHECK(relative_spread < 0.05);

    // NEGATIVE CONTROL.  Move the retrograde FCN resonance by 7e-4 cpsd — less
    // than a part in a thousand — and the ratio stops being a ratio.
    std::array<C, 3> wrong = sigma;
    wrong[1] = C{1.0030000, 0.000025};
    int used2 = 0;
    double lo2 = 0.0, hi2 = 0.0, span2 = 0.0;
    const double median2 = spread_of(wrong, used2, lo2, hi2, span2);
    const double spread2 = (hi2 - lo2) / std::abs(median2);
    WARN("PERT-A-025 negative control: sigma_2 moved from 1.0023181 to 1.0030000 takes the "
         "spread from " << relative_spread * 100.0 << "% to " << spread2 * 100.0 << "%");
    CHECK(spread2 > 10.0 * relative_spread);
}

TEST_CASE("PERT-A-004: step 3 removes the permanent tide, once", "[tides]") {
    // TN36-6 (6.14): dC20^perm = A0 H0 k20.
    const double perm = SolidEarthTide::permanent_c20(LoveNumbers::Anelastic);
    const double by_hand = SolidEarthTide::kA0 * SolidEarthTide::kH0 * 0.30190;
    CHECK_THAT(perm, WithinRel(by_hand, 1e-14));
    WARN("PERT-A-004: A0 H0 k20 = " << perm
         << ", against the -4.1736e-9 TN36-6 §6.2.2 states for EGM2008's own zero-tide minus "
            "tide-free difference — which §6.2.2 reuses rather than recomputing, and which "
            "implies k20 = "
         << 4.1736e-9 / (SolidEarthTide::kA0 * -SolidEarthTide::kH0)
         << " rather than the 0.30190 of Table 6.3.");
    CHECK_THAT(perm, WithinRel(-4.1736e-9, 0.01));      // the same to one part in a hundred

    const frames::Position<frames::Frame::ITRS> sun{Vec3{1.2e11, -8e10, -3e10}};
    const frames::Position<frames::Frame::ITRS> moon{Vec3{-2.8e8, 2.2e8, 1.0e8}};
    const auto args = eop::tides::arguments_at_mjd(58849.0);
    auto zero = SolidEarthTide::increments(sun, moon, args, LoveNumbers::Anelastic,
                                           TideSystem::ZeroTide);
    auto free_ = SolidEarthTide::increments(sun, moon, args, LoveNumbers::Anelastic,
                                            TideSystem::TideFree);
    REQUIRE(zero.has_value());
    REQUIRE(free_.has_value());
    CHECK(zero->system() == TideSystem::ZeroTide);
    CHECK(free_->system() == TideSystem::TideFree);
    // The two differ by exactly the permanent term, in C20 and nowhere else.
    CHECK_THAT(zero->dc(2, 0) - free_->dc(2, 0), WithinRel(-perm, 1e-12));
    CHECK(zero->dc(2, 1) == free_->dc(2, 1));
    CHECK(zero->dc(4, 0) == free_->dc(4, 0));
}

TEST_CASE("PERT-A-022 / A-023: step 1 is body-fixed, and the sizes are right", "[tides]") {
    const frames::Position<frames::Frame::ITRS> sun{Vec3{1.2e11, -8e10, -3e10}};
    const frames::Position<frames::Frame::ITRS> moon{Vec3{-2.8e8, 2.2e8, 1.0e8}};
    auto s1 = SolidEarthTide::step1(sun, moon, LoveNumbers::Anelastic);
    REQUIRE(s1.has_value());
    const double c2 = std::hypot(s1->dc(2, 0), std::hypot(s1->dc(2, 1), s1->dc(2, 2)));
    const double c4 = std::hypot(s1->dc(4, 0), std::hypot(s1->dc(4, 1), s1->dc(4, 2)));
    WARN("PERT-A-022: degree-2 increment " << c2 << ", degree-4 " << c4
         << "; TN36-6 §6.2.1 says degree 2 'can exceed 1e-8' and degree 4 'exceeds 3e-12'");
    CHECK(c2 > 1e-9);
    CHECK(c2 < 1e-7);
    CHECK(c4 > 1e-13);
    CHECK(c4 < 1e-10);

    // PERT-R-012: rotating the bodies about the polar axis — which is what using
    // GCRS coordinates instead of ITRS ones amounts to — changes the answer.
    const double a = 1.0;
    const frames::Position<frames::Frame::ITRS> moon_rot{
        Vec3{moon.metres().x * std::cos(a) - moon.metres().y * std::sin(a),
             moon.metres().x * std::sin(a) + moon.metres().y * std::cos(a), moon.metres().z}};
    auto rotated = SolidEarthTide::step1(sun, moon_rot, LoveNumbers::Anelastic);
    REQUIRE(rotated.has_value());
    CHECK(std::abs(rotated->dc(2, 1) - s1->dc(2, 1)) / std::abs(s1->dc(2, 1)) > 0.1);
    // ... and the zonal term, which has no longitude dependence, does not.
    CHECK_THAT(rotated->dc(2, 0), WithinRel(s1->dc(2, 0), 1e-12));

    // PERT-R-011: the elastic column is a different answer, offered but not default.
    auto elastic = SolidEarthTide::step1(sun, moon, LoveNumbers::Elastic);
    REQUIRE(elastic.has_value());
    CHECK(elastic->dc(2, 0) != s1->dc(2, 0));
    CHECK(s1->models().front().parameters.find("anelastic") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Ocean tides — TN36-6 §6.3
// ---------------------------------------------------------------------------

#include <odl/tides/ocean.hpp>

namespace {
const OceanTide& fes2004() {
    static const auto o = OceanTide::load(ODL_FES2004_CNM_SNM, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(o.has_value());
    return *o;
}
}  // namespace

TEST_CASE("PERT-A-024: the file's header is read, not assumed", "[tides]") {
    const OceanTideHeader& h = fes2004().header();
    WARN("PERT-A-024: " << fes2004().rows() << " coefficient rows, " << fes2004().waves()
         << " waves; model \"" << h.model << "\"");
    CHECK(h.unit == 1e-11);
    CHECK(h.max_degree == 100);
    CHECK(h.long_period_from_fes2002);            // NOT FES2004, whatever §6.3.2 implies
    CHECK(h.long_period_max_degree == 50);
    CHECK(h.includes_equilibrium_omega);          // so §6.3.2's equation must NOT be applied
    CHECK_FALSE(h.includes_atmospheric_tide);
}

TEST_CASE("the Doodson arguments agree with the Delaunay ones, on the tables that print both",
          "[tides]") {
    // TN36-6 Table 6.5a gives BOTH multiplier sets for every diurnal
    // constituent, so the conversion tau = gamma - s, s = F + Omega, ... can be
    // checked against data rather than asserted from a textbook.
    const auto a = eop::tides::arguments_at_mjd(58849.0);
    const std::array<double, 6> beta = doodson_arguments(a);
    const std::array<double, 5> F{a.l, a.lp, a.F, a.D, a.Om};
    int checked = 0;
    double worst = 0.0;
    for (int band = 0; band <= 2; ++band) {
        for (std::size_t i = 0; i < SolidEarthTide::constituents(band); ++i) {
            const odl::tides::tables::SolidTideTerm& t =
                band == 0 ? odl::tides::tables::kSolidTideZonal[i]
              : band == 1 ? odl::tides::tables::kSolidTideDiurnal[i]
                          : odl::tides::tables::kSolidTideSemidiurnal[i];
            double by_doodson = 0.0, by_delaunay = static_cast<double>(t.doodson[0]) * a.gamma;
            for (std::size_t j = 0; j < 6; ++j) by_doodson += t.doodson[j] * beta[j];
            for (std::size_t j = 0; j < 5; ++j) by_delaunay -= t.delaunay[j] * F[j];
            const double d = std::remainder(by_doodson - by_delaunay, 2.0 * std::numbers::pi);
            INFO("band " << band << " row " << i << " at " << t.deg_per_hour << " deg/hr");
            CHECK(std::abs(d) < 1e-9);
            worst = std::max(worst, std::abs(d));
            ++checked;
        }
    }
    WARN("the two argument conventions agree on all " << checked
         << " constituents of Tables 6.5a/b/c, worst difference " << worst << " rad");
    CHECK(checked == 71);
}

TEST_CASE("PERT-A-007: the zonal convention, which is otherwise wrong", "[tides]") {
    const auto a = eop::tides::arguments_at_mjd(58849.0);
    auto inc = fes2004().increments(a, 50, 50);
    REQUIRE(inc.has_value());
    // FES2004 zeroes the retrograde zonal terms and doubles the prograde ones,
    // so dS_n0 must be zero after (6.15).  A build that applies (6.15) uniformly
    // produces a non-zero dS_n0 and says nothing.
    for (int n = 0; n <= 50; ++n) {
        INFO("degree " << n);
        CHECK(inc->ds(n, 0) == 0.0);
    }
    // and dC_n0 is not zero, so the check above is not vacuous
    double zonal = 0.0;
    for (int n = 2; n <= 50; ++n) zonal += std::abs(inc->dc(n, 0));
    WARN("PERT-A-007: sum |dC_n0| over degrees 2 to 50 is " << zonal
         << ", and every dS_n0 is exactly zero");
    CHECK(zonal > 1e-12);
}

TEST_CASE("PERT-A-026: the truncation default, measured against its stated criterion",
          "[tides]") {
    // PERT-R-022a.  The criterion is stated in the specification and the
    // MEASUREMENT sets the default: the truncation error falls below the
    // smallest term the model computes and keeps, at the same point.  Reported
    // at two radii because (ae/r)^n makes it strongly altitude-dependent.
    const OceanTide& o = fes2004();
    int worst_degree = 0;
    for (auto [label, r_m] : {std::pair{"7331 km", 7331e3},
                              std::pair{"300 km altitude", 6378.1363e3 + 300e3}}) {
        const int d = o.degree_meeting_criterion(r_m);
        worst_degree = std::max(worst_degree, d);
        WARN("PERT-A-026 at " << label << ": the criterion is met at degree " << d
             << " — truncation error " << o.truncation_rms(r_m, d) << " m/s^2 against the "
             "smallest kept term " << o.smallest_kept_rms(r_m, d)
             << " m/s^2; at degree 100 the truncation error is "
             << o.truncation_rms(r_m, 100) << " m/s^2");
        CHECK(d >= 2);
        CHECK(d <= 100);
    }
    WARN("PERT-A-026: the default is the LARGER of the two, degree " << worst_degree
         << ". No number was written into the specification before this measurement.");
    // The truncation error must fall as the degree rises, at both radii.
    for (double r_m : {7331e3, 6378.1363e3 + 300e3}) {
        double previous = 1e30;
        for (int d = 2; d <= 100; ++d) {
            const double t = o.truncation_rms(r_m, d);
            INFO("radius " << r_m << " degree " << d);
            CHECK(t <= previous);
            previous = t;
        }
    }
}

TEST_CASE("PERT-F-005 / F-011: ocean tide refusals", "[tides]") {
    const auto a = eop::tides::arguments_at_mjd(58849.0);
    auto too_far = fes2004().increments(a, 120, 120);
    REQUIRE_FALSE(too_far.has_value());
    CHECK(too_far.error().id == "PERT-F-005");
    CHECK(too_far.error().message.find("separate limits") != std::string::npos);

    auto order_above_degree = fes2004().increments(a, 20, 40);
    REQUIRE_FALSE(order_above_degree.has_value());
    CHECK(order_above_degree.error().id == "PERT-F-005");

    auto outside = OceanTide::load("/etc/hostname", ODL_MANIFEST_CACHE_ROOT);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().id == "PERT-F-010");
}
