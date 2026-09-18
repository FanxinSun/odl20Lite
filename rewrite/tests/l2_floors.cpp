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
//
// L2 STEP 4 ADDS A FOURTH ROW, AND IT IS THE ONE THAT BREAKS THE ONE-RADIUS
// RULE ON PURPOSE (SPEC-atmosphere ATMO-R-035).  Drag is the only term in this
// table that varies over ORDERS OF MAGNITUDE across the regime — a factor of
// about ten thousand between 300 km and 950 km — so a single reference point
// would make the table lie in exactly the way §4 rule 3 exists to prevent.  Each
// row states its own reference point; that is the rule working, not a departure
// from it.  And this row carries its UNCERTAINTY as well as its magnitude,
// because for every other row those are the same question and here they are not:
// the atmosphere's model error dominates L4's budget while the other three are
// good to parts in 1e9 or better.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/atmosphere/atmosphere.hpp>
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

// ---------------------------------------------------------------------------

TEST_CASE("L2's atmosphere row: magnitude AND uncertainty, at BOTH radii",
          "[l2][spec][atmosphere]") {
    using namespace odl::atmosphere;

    constexpr double kGm = 3.986004415e14;
    constexpr double kEarthRadiusM = 6378136.3;
    // A representative ballistic coefficient, STATED because it is an assumption
    // this test makes and not a property of the atmosphere: C_D A/m = 0.01 m^2/kg
    // is a compact LEO satellite (C_D ~ 2.2, A/m ~ 0.0045).  L4 owns the real one;
    // what this row fixes is the DENSITY and the ratio, not the vehicle.
    constexpr double kBallistic = 0.01;

    struct Point { const char* name; double altitude_km; };
    const Point points[2] = {{"300 km, where drag dominates everything", 300.0},
                             {"953 km (r = 7331 km), this table's other rows", 952.86}};

    // Solar-moderate, quiet: F10.7 = 150 is the reference's own default and the
    // value its header instructs below 80 km.
    SpaceWeather sw;
    sw.f107_previous_day = 150.0;
    sw.f107a_centred81 = 150.0;
    sw.daily.ap = 4.0;
    sw.snapshot_id = "l2_floors-fixed-conditions";
    sw.snapshot_sha256 = "(not from a snapshot: fixed conditions, stated here)";

    double rho[2] = {0.0, 0.0}, accel[2] = {0.0, 0.0};
    for (int i = 0; i < 2; ++i) {
        Place p;
        p.day_of_year = 172;
        p.seconds_of_day = 29000.0;
        p.geodetic_latitude_deg = 60.0;
        p.longitude_deg = -70.0;
        p.altitude_km = points[i].altitude_km;
        const auto d = for_drag(p, sw);
        REQUIRE(d.has_value());
        rho[i] = d->total_mass_kg_m3;
        const double r = kEarthRadiusM + points[i].altitude_km * 1.0e3;
        const double v = std::sqrt(kGm / r);
        accel[i] = 0.5 * rho[i] * v * v * kBallistic;
    }

    // ATMO-P-4, with the three things a bare percentage hides.  The statistic is
    // the standard deviation of log_e(rho_data/rho_model) from MSIS-STATS Table 1,
    // over data spanning 1963-1997; a fractional 1-sigma error is exp(sigma) - 1.
    const double sigma_quiet_low = 0.07;   // Table 1(a), SETA 79 accel, 120-200 km
    const double sigma_weighted  = 0.172;  // Table 1(c), all levels, 791 314 points
    const double sigma_worst     = 0.97;   // Table 1(b), AE-C MESA accel, 200-400 km, high Ap
    const auto frac = [](double s) { return std::exp(s) - 1.0; };

    WARN("L2 atmosphere row (NRLMSISE-00, day 172, 60 N, F10.7 = F10.7A = 150, Ap = 4)\n"
         << "    " << points[0].name << ":\n"
         << "        density     " << rho[0] << " kg/m^3\n"
         << "        drag accel  " << accel[0] << " m/s^2   (C_D A/m = " << kBallistic << ")\n"
         << "    " << points[1].name << ":\n"
         << "        density     " << rho[1] << " kg/m^3\n"
         << "        drag accel  " << accel[1] << " m/s^2\n"
         << "        DENSITY ratio 300 km : 953 km = " << rho[0] / rho[1] << "\n"
         << "        DRAG    ratio 300 km : 953 km = " << accel[0] / accel[1]
         << "   (the two differ by v^2, and a bare ratio under two drag rows reads\n"
         << "                                          as the drag one)\n"
         << "    UNCERTAINTY -- and it is NOT one number:\n"
         << "        sigma is of log_e(rho_data/rho_model); fractional = exp(sigma) - 1\n"
         << "        quiet, 120-200 km   sigma " << sigma_quiet_low << " -> "
         << 100.0 * frac(sigma_quiet_low) << " %\n"
         << "        all levels, weighted sigma " << sigma_weighted << " -> "
         << 100.0 * frac(sigma_weighted) << " %   (791 314 points)\n"
         << "        high Ap, 200-400 km sigma " << sigma_worst << " -> "
         << 100.0 * frac(sigma_worst) << " %\n"
         << "        data 1963-1997. THIS IS MODEL-MINUS-DATA SCATTER INCLUDING DATA NOISE --\n"
         << "        an UPPER BOUND on model error, not model error. The paper says so, and\n"
         << "        the three published models' sigmas agree to ~5 % of their value, which\n"
         << "        rules out a sigma that is mostly model-SPECIFIC but cannot show the\n"
         << "        model term is small.\n"
         << "    AGAINST THIS LAYER'S OTHER ROWS -- the comparison the row exists for:\n"
         << "        ocean-tide truncation floor      8.552e-11 m/s^2\n"
         << "        smallest term kept (de Sitter)   3.478e-11 m/s^2\n"
         << "        drag at 300 km is " << accel[0] / 8.552e-11 << "x the floor and "
         << accel[0] / 3.478e-11 << "x the smallest kept term\n"
         << "        drag at 953 km is " << accel[1] / 8.552e-11 << "x the floor and "
         << accel[1] / 3.478e-11 << "x the smallest kept term\n"
         << "    So drag is above the floor at BOTH radii, by four orders of magnitude at\n"
         << "    one of them and by less than one at the other. Neither number alone is the\n"
         << "    fact, and its uncertainty above dwarfs the spacing of everything below it.");

    // THE RATIO IS THE FACT L4 NEEDS, and a single reference point cannot carry it.
    // BOTH ratios are reported because they are different numbers: the drag ratio
    // carries the v^2 the density ratio does not, and an unlabelled ratio printed
    // under two drag rows will be read as the drag one.
    CHECK(rho[0] / rho[1] > 1.0e3);
    CHECK(rho[0] / rho[1] < 1.0e5);
    CHECK(accel[0] / accel[1] > rho[0] / rho[1]);   // v is larger at the lower altitude

    // At 300 km drag dwarfs everything else this layer computes; at 953 km it is
    // still far above the floor. Both are true and neither alone is the fact.
    CHECK(accel[0] > 1.0e-7);
    CHECK(accel[1] > 8.552e-11);

    // the uncertainty spans more than a factor of eight across the table, which is
    // why ATMO-P-4 refuses to be quoted as a single percentage
    CHECK(frac(sigma_worst) / frac(sigma_quiet_low) > 8.0);
}
