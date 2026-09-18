// l2_floors.cpp — the three numbers that must be comparable, in one place.
//
// SPEC-perturbations and SPEC-ephemerides each stated one of these against its
// own reference point, and the two disagreed by three orders of magnitude:
// EPH-P-5 said the unapplied L_B scaling is 5.9e-4 of "the smallest term L2
// step 3 keeps" — which was the OCEAN-TIDE FLOOR — while PERT-R-022a said the
// layer keeps relativistic terms three orders BELOW that floor.  Both cannot be
// true.  The "three orders" was wrong: it compared a de Sitter term at
// GEOSTATIONARY with a truncation floor at LEO.
//
// So the three are measured here, at ONE radius, from the modules themselves,
// and the comparison is printed rather than reconstructed.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/core/units.hpp>
#include <odl/relativity/correction.hpp>

#include <cmath>
#include <numbers>

using namespace odl;
using frames::Frame;

TEST_CASE("L2's floors, at one radius and with one reference point", "[l2][spec]") {
    constexpr double kR = 7331e3;                  // m, TN36-6 Table 6.1's LEO row
    constexpr double kGm = 3.986004415e14;         // EGM2008's, TT-compatible
    constexpr double kAe = 6378136.3;              // EGM2008's reference radius
    constexpr double kLb = 1.550519768e-8;         // TN36-1

    const double newtonian = kGm / (kR * kR);

    // 1. THE OCEAN-TIDE TRUNCATION FLOOR.  TN36-6 §6.2.1's own accuracy cutoff
    //    for what the solid Earth tide includes — changes "exceeding 3e-12" in
    //    C4m — through GRAV-R-041's identity at this radius.
    const double ratio4 = std::pow(kAe / kR, 4.0);
    const double floor_ = newtonian * ratio4 * 3e-12 * std::sqrt(5.0 * 9.0);

    // 2. THE SMALLEST RELATIVISTIC TERM THIS LAYER ACTUALLY COMPUTES, from the
    //    module rather than from TN36-10's stated band.
    auto epoch = odl::time::Epoch::from_gps_week(2000, 0.0);
    REQUIRE(epoch.has_value());
    const double v = std::sqrt(kGm / kR);
    const frames::State<Frame::GCRS> sat{*epoch, odl::km_from_metres(Vec3{kR, 0.0, 0.0}),
                                         odl::km_from_metres(Vec3{0.0, v * 0.8, v * 0.6})};
    const double R = 1.495978707e11;
    const double V = std::sqrt(relativity::Correction::kGmSun / R);
    const frames::State<Frame::BCRS> earth{*epoch, odl::km_from_metres(Vec3{R, 0.0, 0.0}),
                                           odl::km_from_metres(Vec3{0.0, V, 0.0})};
    auto parts = relativity::Correction::by_term(sat, earth);
    REQUIRE(parts.has_value());
    double smallest_kept = 1e300;
    const char* smallest_name = "";
    for (const auto& p : *parts) {
        if (p.a_m_s2.norm() < smallest_kept) {
            smallest_kept = p.a_m_s2.norm();
            smallest_name = relativity::name_of(p.term);
        }
    }

    // 3. THE UNAPPLIED L_B SCALING, as a third-body acceleration.  The Moon's
    //    contribution at this radius is about 1.09e-6 m/s^2 and the scaling
    //    enters the acceleration three times over, through 1/r^3 times r.
    constexpr double kMoonThirdBody = 1.09e-6;
    const double lb_effect = 3.0 * kLb * kMoonThirdBody;

    WARN("L2 floors at r = " << kR / 1e3 << " km, main Newtonian acceleration " << newtonian
         << " m/s^2:\n"
         "    ocean-tide truncation floor (TN36-6 §6.2.1's 3e-12 cutoff)  " << floor_ << " m/s^2\n"
         "    smallest relativistic term this layer computes (" << smallest_name << ")   "
         << smallest_kept << " m/s^2\n"
         "    unapplied L_B scaling, as a third-body acceleration          " << lb_effect
         << " m/s^2\n"
         "    the floor is " << floor_ / smallest_kept << "x the smallest term kept, and the L_B "
         "effect is " << lb_effect / smallest_kept << " of it.");

    // The asymmetry PERT-R-022a states: the floor is ABOVE the smallest term
    // this layer keeps, because it applies to a quadratic-cost series and not to
    // a closed form.  It is a factor of a few, NOT three orders of magnitude —
    // the "three orders" compared a geostationary term with a LEO floor.
    CHECK(floor_ > smallest_kept);
    CHECK(floor_ / smallest_kept > 1.5);
    CHECK(floor_ / smallest_kept < 20.0);

    // And the conclusion EPH-P-5 rests on, against the right reference point:
    // the unapplied scaling is far below the SMALLEST TERM KEPT, not merely
    // below the truncation floor.
    CHECK(lb_effect < smallest_kept);
    CHECK(lb_effect / smallest_kept < 0.01);
}
