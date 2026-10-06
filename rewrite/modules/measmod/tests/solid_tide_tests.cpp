// solid_tide_tests.cpp — SPEC-measmod.md §8.8, §8.9: MEAS-A-101a (the test-side solid-tide helper against the printed test cases of the IERS routine's prolog, within the stated uncertainty
// written BEFORE the comparison; registered, run ONCE) and MEAS-A-101b (the helper's closed forms, run freely).
//
// The published cases are the test cases printed in the prolog of the IERS Conventions software collection's `DEHANTTIDEINEL.F` (TN36 chapter 7, §7.1.1; the routine named and cited, NO routine
// text): the printed station, Sun and Moon positions (ECEF, metres) and the printed displacement, used here as observations. The helper is the Step-1 degree-2 term; the routine is the whole of
// Steps 1 and 2; the difference is the helper's stated uncertainty U_h = 30 mm (SPEC-measmod §8.9: the sizes of the omitted terms, from TN36 chapter 7's own statements, written down before the
// comparison). THE FOURTH PRINTED CASE IS INCONSISTENT AS PRINTED (its expected output equals the third's to every printed digit, with a different epoch, station, Sun and Moon, and its Sun is
// at a tenth of its distance): it constrains nothing, and this file shows that rather than fitting it.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "solid_tide.hpp"

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

using namespace odl::measmod::testing;
using odl::Vec3;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kUh_m = 0.030;                       // the helper's stated uncertainty, SPEC-measmod §8.9 (24.4 mm radial + 5.0 mm transverse = 29.4 mm, frozen as 30 mm)

struct PrintedCase {
    const char* date;                                 // UTC, 0 h
    Vec3 station, sun, moon, expected;                // ECEF metres; the displacement in metres, as printed
};

// The four test cases as printed in the prolog (digits as printed).
const std::array<PrintedCase, 4> kPrinted = {{
    {"2009-04-13", {4075578.385, 931852.890, 4801570.154}, {137859926952.015, 54228127881.4350, 23509422341.6960}, {-179996231.920342, -312468450.131567, -169288918.592160},
     {0.7700420357108125891e-01, 0.6304056321824967613e-01, 0.5516568152597246810e-01}},
    {"2012-07-13", {1112189.660, -4842955.026, 3985352.284}, {-54537460436.2357, 130244288385.279, 56463429031.5996}, {300396716.912, 243238281.451, 120548075.939},
     {-0.2036831479592075833e-01, 0.5658254776225972449e-01, -0.7597679676871742227e-01}},
    {"2015-07-15", {1112200.5696, -4842957.8511, 3985345.9122}, {100210282451.6279, 103055630398.3160, 56855096480.4475}, {369817604.4348, 1897917.5258, 120804980.8284},
     {0.00509570869172363845, 0.0828663025983528700, -0.0636634925404189617}},
    {"2017-01-15", {1112152.8166, -4842857.5435, 3985496.1783}, {8382471154.1312895, 10512408445.356153, -5360583240.3763866}, {380934092.93550891, 2871428.1904491195, 79015680.553570181},
     {0.0050957086917236384, 0.082866302598352870, -0.063663492540418962}},
}};

double max_component(const Vec3& v) { return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)}); }

}  // namespace

TEST_CASE("MEAS-A-101a  the test-side solid-tide helper (the IERS Step-1 degree-2 term) against the printed test cases of the DEHANTTIDEINEL prolog, within its stated uncertainty of 30 mm written "
          "before the comparison: the three consistent cases agree in norm and in every component; a deliberate sign flip misses them by at least three times that; the fourth printed case is "
          "inconsistent as printed",
          "[measmod][tidepub][pregistered]") {
    SECTION("the three consistent cases, once") {
        for (std::size_t i = 0; i < 3; ++i) {
            const PrintedCase& c = kPrinted[i];
            const Vec3 helper = solid_tide_step1_degree2(c.station, c.moon, c.sun);
            const Vec3 diff = helper - c.expected;
            const Vec3 flipped = (-1.0 * helper) - c.expected;
            WARN(std::setprecision(5) << "case " << i + 1 << " (" << c.date << "): printed (" << c.expected.x << ", " << c.expected.y << ", " << c.expected.z << ") m, |" << c.expected.norm()
                                      << "|; helper (" << helper.x << ", " << helper.y << ", " << helper.z << "); difference (" << diff.x * 1e3 << ", " << diff.y * 1e3 << ", " << diff.z * 1e3
                                      << ") mm, norm " << diff.norm() * 1e3 << " mm of the " << kUh_m * 1e3 << " allowed; the sign-flipped helper misses it by " << flipped.norm() * 1e3 << " mm");
            INFO("case " << i + 1 << ", " << c.date);
            CHECK(diff.norm() <= kUh_m);
            CHECK(max_component(diff) <= kUh_m);
            CHECK(flipped.norm() >= 3.0 * kUh_m);                  // rule 5: a sign slip is far outside the uncertainty
        }
    }
    SECTION("the fourth printed case is inconsistent as printed, and constrains nothing") {
        const PrintedCase& c3 = kPrinted[2];
        const PrintedCase& c4 = kPrinted[3];
        // its expected output equals the third's to every printed digit ...
        CHECK((c4.expected - c3.expected).norm() <= 1e-15);
        // ... with a different epoch, station, Sun and Moon
        CHECK((c4.station - c3.station).norm() > 100.0);
        CHECK((c4.sun - c3.sun).norm() > 1e10);
        CHECK((c4.moon - c3.moon).norm() > 1e7);
        // and its Sun is at 1.4e10 m, a tenth of the Sun's distance in January (1.47e11 m): the printed vector cannot be the Sun's position
        CHECK(c4.sun.norm() < 5e10);
        CHECK(c3.sun.norm() > 1.4e11);
        // the helper's displacement for the case as printed is reported, and is NOT a criterion: the Sun's tide at a tenth of its distance is a thousand times larger
        const Vec3 helper = solid_tide_step1_degree2(c4.station, c4.moon, c4.sun);
        WARN(std::setprecision(5) << "case 4 (" << c4.date << ") as printed: helper (" << helper.x << ", " << helper.y << ", " << helper.z << ") m against the printed " << c4.expected.norm()
                                  << " m (the Sun printed at " << c4.sun.norm() << " m): not compared");
    }
}

TEST_CASE("MEAS-A-101b  the tide helper's closed forms: the Moon at the zenith gives +h2 k_Moon, at the horizon -h2 k_Moon/2, the Moon and the Sun together +0.3179 m; the horizontal maximum "
          "at 45 degrees is 1.5 l2 k; a body and its antipode give the same displacement; the displacement rotates with the frame",
          "[measmod][tidecf]") {
    const TideConstants c;
    const double r_moon = 384400.0e3, r_sun = 1.495978707e11;
    const double k_moon = c.moon_earth_mass_ratio * std::pow(c.re_m / r_moon, 3.0) * c.re_m;
    const double k_sun = (c.gm_sun / c.gm_earth) * std::pow(c.re_m / r_sun, 3.0) * c.re_m;
    const Vec3 station{c.re_m, 0.0, 0.0};
    const Vec3 up{1.0, 0.0, 0.0}, north{0.0, 0.0, 1.0};
    const Vec3 far_sun = 1.0e30 * north;                    // a Sun that contributes nothing

    SECTION("the budget rows' numbers (MEAS-P-18 .. -21)") {
        CHECK_THAT(k_moon, WithinRel(0.3584, 1e-3));         // MEAS-P-18
        CHECK_THAT(k_sun, WithinRel(0.1646, 1e-3));          // MEAS-P-19
        CHECK_THAT(c.h2 * (k_moon + k_sun), WithinRel(0.3179, 1e-3));          // MEAS-P-20
        CHECK_THAT(1.5 * c.l2 * (k_moon + k_sun), WithinRel(0.0664, 1e-3));    // MEAS-P-21
    }
    SECTION("the Moon at the zenith: +h2 k_Moon, radial, no transverse part") {
        const Vec3 d = solid_tide_step1_degree2(station, r_moon * up, far_sun);
        CHECK_THAT(d.x, WithinAbs(c.h2 * k_moon, 1e-9));
        CHECK_THAT(d.y, WithinAbs(0.0, 1e-12));
        CHECK_THAT(d.z, WithinAbs(0.0, 1e-9));
        CHECK(d.x > 0.0);                                    // outward, toward the tide-raising body
    }
    SECTION("the Moon at the horizon: -h2 k_Moon / 2, and the antipode gives the same") {
        const Vec3 d = solid_tide_step1_degree2(station, r_moon * north, far_sun);
        CHECK_THAT(d.x, WithinAbs(-0.5 * c.h2 * k_moon, 1e-9));
        CHECK_THAT(d.z, WithinAbs(0.0, 1e-9));
        const Vec3 d2 = solid_tide_step1_degree2(station, -r_moon * north, far_sun);
        CHECK((d - d2).norm() <= 1e-12);
        const Vec3 d3 = solid_tide_step1_degree2(station, -r_moon * up, far_sun);
        const Vec3 d4 = solid_tide_step1_degree2(station, r_moon * up, far_sun);
        CHECK((d3 - d4).norm() <= 1e-12);
    }
    SECTION("the Moon and the Sun both at the zenith: +h2 (k_Moon + k_Sun) = 0.3179 m") {
        const Vec3 d = solid_tide_step1_degree2(station, r_moon * up, r_sun * up);
        CHECK_THAT(d.x, WithinAbs(c.h2 * (k_moon + k_sun), 1e-9));
    }
    SECTION("45 degrees from the zenith: radial h2 k/4, transverse 1.5 l2 k toward the body's side") {
        // R-hat = (s, 0, s), r-hat = (1, 0, 0), cos(psi) = s = sqrt(1/2): radial h2 (3 cos^2 - 1)/2 = h2/4 along x, transverse 3 l2 cos (R-hat - cos r-hat) = 3 l2 s (0, 0, s) = 1.5 l2 along z
        const double s = std::sqrt(0.5);
        const Vec3 d = solid_tide_step1_degree2(station, r_moon * (s * up + s * north), far_sun);
        CHECK_THAT(d.x, WithinAbs(c.h2 * k_moon / 4.0, 1e-9));
        CHECK_THAT(d.y, WithinAbs(0.0, 1e-12));
        CHECK_THAT(d.z, WithinAbs(1.5 * c.l2 * k_moon, 1e-9));
        CHECK(d.z > 0.0);                                    // toward the body
    }
    SECTION("the displacement rotates with the frame (the vector algebra does not depend on the axes)") {
        const Vec3 moon{2.3e8, -1.9e8, 2.2e8}, sun{-1.2e11, 7.0e10, 5.0e10}, sta{3.9e6, -3.1e6, 3.8e6};
        const Vec3 d = solid_tide_step1_degree2(sta, moon, sun);
        // a rotation of 1.1 rad about the axis (1, 2, 3)/sqrt(14), Rodrigues
        const Vec3 k = (1.0 / std::sqrt(14.0)) * Vec3{1.0, 2.0, 3.0};
        const double th = 1.1;
        auto rot = [&](const Vec3& v) { return std::cos(th) * v + std::sin(th) * k.cross(v) + ((1.0 - std::cos(th)) * k.dot(v)) * k; };
        const Vec3 dr = solid_tide_step1_degree2(rot(sta), rot(moon), rot(sun));
        CHECK((dr - rot(d)).norm() <= 1e-12);
    }
}
