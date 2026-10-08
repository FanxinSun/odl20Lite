// troposphere_tests.cpp — SPEC-measmod.md §8.3, MEAS-A-020 … MEAS-A-024.
//
// G2: the three term-level cases the IERS Conventions routines print in their comment prologs (FCUL_ZD_HPA, FCUL_A, FCUL_B),
// used as published observations with the routine named and cited; no routine text is in the tree. The zenith delay's
// printed value is asserted to the source's own stated accuracy (1 mm) and the 3.8 µm by which the PRINTED EQUATIONS miss it is
// reported beside it, not pursued. The expected values of the equations come from tools/measmod_reference.cpp (60 digits).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/measmod/troposphere.hpp>

#include "measmod_reference.hpp"

#include <cmath>
#include <iomanip>
#include <limits>

using namespace odl::measmod;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kPi = 3.14159265358979323846;
double sin_deg(double d) { return std::sin(d * kPi / 180.0); }

// the station of the IERS FCUL_A / FCUL_B / FCUL_ZD_HPA test cases (McDonald Observatory): the latitude as the generator formed it
const double kLat = ref::zenith_lat_rad;

}  // namespace

TEST_CASE("MEAS-A-020  G2, the zenith delay: the IERS FCUL_ZD_HPA printed case within 1 mm (the source's stated accuracy), the "
          "implementation equal to an independent 60-digit evaluation of the printed equations, the 3.8 um gap reported",
          "[measmod][troposphere][published]") {
    // IERS Conventions (2010) chapter 9 software, FCUL_ZD_HPA.F prolog, test case: lat 30.67166667 deg, ellipsoidal height 2010.344 m,
    // P 798.4188 hPa, water-vapour pressure 14.322 hPa, wavelength 0.532 um. Expected output as printed to 19 digits:
    constexpr double printed_total = 1.935225924846803114, printed_hydrostatic = 1.932992176591644462, printed_wet = 0.2233748255158703871e-2;
    auto z = zenith_delay(ref::zenith_lat_rad, ref::zenith_height_m, ref::zenith_p_hpa, ref::zenith_e_hpa, ref::zenith_lambda_um);
    REQUIRE(z.has_value());
    // asserted: the source's own stated accuracy, "overall rms errors for the total zenith delay below 1 mm" (TN36 §9.1.1; MEAS-P-11)
    CHECK_THAT(z->total_m, WithinAbs(printed_total, 1e-3));
    CHECK_THAT(z->hydrostatic_m, WithinAbs(printed_hydrostatic, 1e-3));
    CHECK_THAT(z->wet_m, WithinAbs(printed_wet, 1e-3));
    // also asserted: the implementation IS the printed equations (so the 1 mm is not hiding an implementation error)
    CHECK_THAT(z->total_m, WithinAbs(ref::zenith_total_m, 1e-12));
    CHECK_THAT(z->hydrostatic_m, WithinAbs(ref::zenith_hydrostatic_m, 1e-12));
    CHECK_THAT(z->wet_m, WithinAbs(ref::zenith_wet_m, 1e-12));
    CHECK_THAT(z->total_m, WithinAbs(z->hydrostatic_m + z->wet_m, 0.0));
    // REPORTED beside it, not gated: the printed equations miss the printed number by +3.8 um (ZHD +3.796 um, ZWD +4.5 nm), 2.0 ppm.
    // The cause was not looked for: it would mean reading the routine's body (ruling R1, the manager's step-3 rule).
    const double gap = z->total_m - printed_total;
    WARN(std::setprecision(16) << "the printed equations give " << z->total_m << " m against the printed " << printed_total << " m: " << gap * 1e6 << " um ("
         << (z->hydrostatic_m - printed_hydrostatic) * 1e6 << " um hydrostatic, " << (z->wet_m - printed_wet) * 1e9 << " nm wet), "
         << gap / printed_total * 1e6 << " ppm; not pursued");
    CHECK_THAT(gap, WithinAbs(3.80e-6, 0.02e-6));                 // the gap this implementation shows is the one the round-8 arithmetic found
    CHECK_THAT(z->hydrostatic_m - printed_hydrostatic, WithinAbs(3.796e-6, 0.002e-6));
    CHECK_THAT(z->wet_m - printed_wet, WithinAbs(4.5e-9, 0.1e-9));
}

TEST_CASE("MEAS-A-021  G2, the FCULa mapping function: the IERS FCUL_A printed case",
          "[measmod][troposphere][published]") {
    // FCUL_A.F prolog, test case: lat 30.67166667 deg, height 2075 m, T 300.15 K, elevation 15 deg. Printed: 3.800243667312344087
    const double ts = 300.15 - 273.15;                             // degrees Celsius, as the case's own temperature gives it
    const Mapping m = mapping_fcula(sin_deg(15.0), kLat, 2075.0, ts);
    CHECK_THAT(m.value, WithinAbs(3.800243667312344087, 1e-12));
    // the derivative with respect to sin e (used by the partials): compared with central differences of the function itself
    for (double h : {1e-4, 1e-5, 1e-6}) {
        const double s = sin_deg(15.0);
        const double fd = (mapping_fcula(s + h, kLat, 2075.0, ts).value - mapping_fcula(s - h, kLat, 2075.0, ts).value) / (2.0 * h);
        CHECK_THAT(m.d_dsin_e, WithinRel(fd, 1e-6));
    }
    // normalised to unity at the zenith (TN36 §9.1.2): m(90 deg) = 1
    CHECK_THAT(mapping_fcula(1.0, kLat, 2075.0, ts).value, WithinAbs(1.0, 1e-15));
    // and it grows as the elevation falls
    double previous = 0.0;
    for (double e : {90.0, 60.0, 40.0, 20.0, 10.0, 5.0, 3.0}) {
        const double v = mapping_fcula(sin_deg(e), kLat, 2075.0, ts).value;
        CHECK(v > previous);
        previous = v;
    }
}

TEST_CASE("MEAS-A-022  the water-vapour pressure from relative humidity, Marini & Murray (1973) eq. (22), against an independent "
          "evaluation at four (t, Rh)",
          "[measmod][troposphere]") {
    CHECK_THAT(water_vapour_pressure_hpa(ref::vapour_a_rh, ref::vapour_a_t_c), WithinRel(ref::vapour_a_e_hpa, 1e-12));
    CHECK_THAT(water_vapour_pressure_hpa(ref::vapour_b_rh, ref::vapour_b_t_c), WithinRel(ref::vapour_b_e_hpa, 1e-12));
    CHECK_THAT(water_vapour_pressure_hpa(ref::vapour_c_rh, ref::vapour_c_t_c), WithinRel(ref::vapour_c_e_hpa, 1e-12));
    CHECK_THAT(water_vapour_pressure_hpa(ref::vapour_d_rh, ref::vapour_d_t_c), WithinRel(ref::vapour_d_e_hpa, 1e-12));
    CHECK(water_vapour_pressure_hpa(1.0, 0.0) == 6.11);                 // at 0 degrees C the exponent is 0: the formula's own constant
    CHECK(water_vapour_pressure_hpa(0.0, 20.0) == 0.0);                 // no humidity, no vapour
    // monotonic in both arguments
    double prev = -1.0;
    for (double rh = 0.0; rh <= 1.0; rh += 0.1) {
        const double e = water_vapour_pressure_hpa(rh, 15.0);
        CHECK(e > prev);
        prev = e;
    }
    prev = 0.0;
    for (double t = -40.0; t <= 50.0; t += 5.0) {
        const double e = water_vapour_pressure_hpa(0.5, t);
        CHECK(e > prev);
        prev = e;
    }
}

TEST_CASE("MEAS-A-023  G2, the FCULb mapping function: the IERS FCUL_B printed case, and a phi_d in place of phi_d squared is shown "
          "failing it",
          "[measmod][troposphere][published]") {
    // FCUL_B.F prolog, test case: lat 30.67166667 deg, height 2075 m, day of year 224, elevation 15 deg. Printed: 3.800758725284345996
    constexpr double printed = 3.800758725284345996;
    const double got = mapping_fculb(sin_deg(15.0), kLat, 2075.0, 224.0);
    CHECK_THAT(got, WithinAbs(printed, 1e-12));
    // NEGATIVE CONTROL (plan §4 rule 5). Mendes et al. (2002) eq. (6) has phi_d SQUARED, which the PDF's text layer renders ambiguously
    // (it also drops every minus sign of Table 1): a reading with phi_d instead misses the printed case by 2.5e-4, and the same assertion
    // fails for it. The coefficients are the published constants, repeated here so the variant can be built.
    const double phi_d = kLat * 180.0 / kPi, cphi = std::cos(kLat);
    const double season = std::cos(2.0 * kPi / 365.25 * (224.0 - 28.0)), H = 2075.0, s = sin_deg(15.0);
    const double a1 = 11613.1e-7 + (-933.8e-8 + -595.8e-11 * phi_d) * season - 2462.7e-11 * H + 1286.4e-7 * cphi;
    const double a2 = 29815.1e-7 + (-56.9e-7 + -165.5e-10 * phi_d) * season - 272.5e-10 * H + 302.0e-7 * cphi;
    const double a3 = 68183.9e-6 + (93.5e-6 + -239.4e-9 * phi_d) * season + 30.4e-9 * H - 230.8e-5 * cphi;
    const double wrong = (1.0 + a1 / (1.0 + a2 / (1.0 + a3))) / (s + a1 / (s + a2 / (s + a3)));
    CHECK(std::abs(wrong - printed) > 1e-4);
    CHECK(std::abs(wrong - printed) < 5e-4);
    CHECK_FALSE(std::abs(wrong - printed) <= 1e-12);                    // the very assertion above, applied to the misreading, fails
    // FCULb and FCULa are two fits of the same quantity: they agree to 5e-4 at 15 degrees (the printed cases differ by 5.1e-4)
    const double fcula = mapping_fcula(s, kLat, H, 300.15 - 273.15).value;
    CHECK_THAT(got, WithinAbs(fcula, 6e-4));
}

TEST_CASE("MEAS-A-024  the leg delay ZTD x m(sin e), the 3 degree limit (MEAS-F-010), the measured height sensitivity, and the "
          "atmosphere validator (MEAS-F-022)",
          "[measmod][troposphere]") {
    // the system reference point of Yarragadee at the first normal point (MEAS-A-010) and that normal point's weather (the real file's record 20)
    const double lat = ref::yarl_srp_lat_rad, h = ref::yarl_srp_h_m;
    const Atmosphere atm{976.70, 310.30 - 273.15, 0.12};
    auto model = TroposphereModel::make(lat, h, atm, 0.532);
    REQUIRE(model.has_value());
    const double e = water_vapour_pressure_hpa(atm.relative_humidity, atm.temperature_c);
    auto z = zenith_delay(lat, h, atm.pressure_hpa, e, 0.532);
    REQUIRE(z.has_value());
    CHECK(model->zenith_total_m() == z->total_m);
    CHECK(model->water_vapour_hpa() == e);

    // the leg delay is the product of the two tested terms, at several elevations
    for (double elev : {90.0, 60.0, 40.0, 15.0, 5.0, 3.0}) {
        const double s = sin_deg(elev);
        auto leg = model->leg(s);
        INFO("elevation " << elev);
        REQUIRE(leg.has_value());
        const Mapping m = mapping_fcula(s, lat, h, atm.temperature_c);
        CHECK(leg->mapping == m.value);
        CHECK(leg->ztd_m == z->total_m);
        CHECK_THAT(leg->delay_m, WithinAbs(z->total_m * m.value, 1e-12));
        CHECK_THAT(leg->d_delay_d_sin_e, WithinAbs(z->total_m * m.d_dsin_e, 1e-12));
    }
    CHECK_THAT(model->leg(1.0)->delay_m, WithinAbs(z->total_m, 1e-12));        // at the zenith the mapping is 1

    // the elevation limit: 3.0 deg accepted, 2.99 deg refused, the horizon and below refused, NaN refused
    CHECK(model->leg(sin_deg(3.0)).has_value());
    CHECK(model->leg(sin_deg(3.0001)).has_value());
    for (double bad : {sin_deg(2.99), sin_deg(2.0), 0.0, -0.1, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
        auto r = model->leg(bad);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-010");
        CHECK_THAT(r.error().message, ContainsSubstring("3"));
    }

    // THE HEIGHT SENSITIVITY IS MEASURED (TN36 §9.1.1 footnote 1 says the formula is "insensitive" to geodetic against orthometric height).
    // H enters twice: the zenith delay's f_s and the mapping function's a_i3 H. Predicted before this ran, by tools/measmod_height_sensitivity.py (now tools/measmod_height_sensitivity.cpp)
    // (the IERS case's inputs, T = 15 C, 15 deg elevation): 0.1146 mm for 30 m and 0.3819 mm for 100 m.
    auto leg_at = [&](double height) {
        auto zz = zenith_delay(kLat, height, ref::zenith_p_hpa, ref::zenith_e_hpa, ref::zenith_lambda_um);
        REQUIRE(zz.has_value());
        return zz->total_m * mapping_fcula(sin_deg(15.0), kLat, height, 15.0).value;
    };
    const double d30 = leg_at(244.0 + 30.0) - leg_at(244.0), d100 = leg_at(244.0 + 100.0) - leg_at(244.0);
    WARN("height sensitivity of the one-way delay at 15 degrees: " << d30 * 1e3 << " mm for 30 m, " << d100 * 1e3 << " mm for 100 m (predicted 0.115 and 0.382)");
    CHECK(std::abs(d30) < 2e-4);                                  // the criterion: under 0.2 mm for a typical geoid undulation …
    CHECK(std::abs(d100) < 5e-4);                                 // … and under 0.5 mm for 100 m, both under the zenith model's own 1 mm
    CHECK_THAT(d30, WithinAbs(1.146e-4, 2e-7));                   // and the prediction written before the run held
    CHECK_THAT(d100, WithinAbs(3.819e-4, 2e-7));

    // MEAS-F-022: the validator and the model's make() refuse a non-physical atmosphere; the neighbours of every limit pass
    struct Case { Atmosphere a; double lam; };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Case bad_cases[] = {{{0.0, 15.0, 0.5}, 0.532}, {{-1.0, 15.0, 0.5}, 0.532}, {{nan, 15.0, 0.5}, 0.532}, {{1013.0, -100.1, 0.5}, 0.532},
                              {{1013.0, 60.1, 0.5}, 0.532}, {{1013.0, nan, 0.5}, 0.532}, {{1013.0, 15.0, -0.01}, 0.532}, {{1013.0, 15.0, 1.01}, 0.532},
                              {{1013.0, 15.0, 0.5}, 0.0}, {{1013.0, 15.0, 0.5}, -0.5}, {{1013.0, 15.0, 0.5}, nan}};
    for (const Case& c : bad_cases) {
        auto v = validate_atmosphere(c.a, c.lam);
        REQUIRE_FALSE(v.has_value());
        CHECK(v.error().id == "MEAS-F-022");
        auto m = TroposphereModel::make(lat, h, c.a, c.lam);
        REQUIRE_FALSE(m.has_value());
        CHECK(m.error().id == "MEAS-F-022");
    }
    const Case good_cases[] = {{{1013.0, -100.0, 0.5}, 0.532}, {{1013.0, 60.0, 0.5}, 0.532}, {{1013.0, 15.0, 0.0}, 0.532}, {{1013.0, 15.0, 1.0}, 0.532},
                               {{0.001, 15.0, 0.5}, 0.532}, {{1013.0, 15.0, 0.5}, 1e-3}};
    for (const Case& c : good_cases) CHECK(validate_atmosphere(c.a, c.lam).has_value());
    // the zenith delay itself refuses a negative vapour pressure and a non-positive wavelength
    CHECK_FALSE(zenith_delay(lat, h, 1013.0, -1.0, 0.532).has_value());
    CHECK_FALSE(zenith_delay(lat, h, 1013.0, 10.0, 0.0).has_value());
    CHECK(zenith_delay(lat, h, 1013.0, 0.0, 0.532).has_value());
}
