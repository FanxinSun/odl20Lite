// toolchain_smoke.cpp — what L0 has to prove about the toolchain before L1 can
// rest any numeric claim on it.
//
// These are not physics tests; there is no physics yet.  They are the platform
// preconditions that the ADOPTED specifications already assume.  SPEC-time §4.2
// disqualifies a bare f64 Julian Date by computing the spacing of representable
// doubles near JD 2.46e6 and turning it into 0.30 m of along-track position at
// LEO.  That argument is only sound if this platform's double is IEEE 754
// binary64 and if the compiler is not quietly evaluating at higher precision.
// Both are checked here, so that a toolchain change that would invalidate an
// adopted spec fails a test rather than going unnoticed.
//
// The second thing proved here is that Catch2's matcher vocabulary covers the
// tolerance vocabulary the specs are written in — WithinAbs for "< 1 mm",
// WithinRel for "relative 1e-6", WithinULP for "bit-comparable".  That match is
// why Catch2 was chosen over doctest and GoogleTest, so it is worth a test
// rather than a claim.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cfloat>
#include <cmath>
#include <limits>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinULP;

namespace {
constexpr double kSecondsPerDay = 86400.0;
constexpr double kLeoSpeed_m_s = 7500.0;   // SPEC-time §6's conversion basis
}  // namespace

TEST_CASE("double is IEEE 754 binary64", "[toolchain][precision]") {
    // Every precision claim in SPEC-time rests on this.
    STATIC_REQUIRE(std::numeric_limits<double>::is_iec559);
    STATIC_REQUIRE(std::numeric_limits<double>::digits == 53);
    STATIC_REQUIRE(std::numeric_limits<double>::radix == 2);
    STATIC_REQUIRE(sizeof(double) == 8);
}

TEST_CASE("no excess intermediate precision", "[toolchain][precision]") {
    // FLT_EVAL_METHOD 2 means doubles are evaluated as long double, which would
    // make the spacing arithmetic below true in the abstract and false in the
    // generated code.  0 or 1 is required; on x86-64 SSE2 it is 0.
    REQUIRE(FLT_EVAL_METHOD >= 0);
    REQUIRE(FLT_EVAL_METHOD <= 1);
}

TEST_CASE("SPEC-time §4.2's representation disqualifications hold on this platform",
          "[toolchain][precision][spec-time]") {
    SECTION("f64 Julian Date spacing is ~40 us, i.e. ~0.30 m at LEO") {
        const double jd = 2460000.0;                 // a JD in the era of interest
        const double ulp = std::nextafter(jd, std::numeric_limits<double>::infinity()) - jd;
        REQUIRE_THAT(ulp, WithinRel(std::ldexp(1.0, -31), 1e-12));

        const double micros = ulp * kSecondsPerDay * 1e6;
        REQUIRE_THAT(micros, WithinAbs(40.2, 0.1));

        const double along_track_m = ulp * kSecondsPerDay * kLeoSpeed_m_s;
        REQUIRE_THAT(along_track_m, WithinAbs(0.302, 0.01));
        REQUIRE(along_track_m > 0.05);   // the disqualification: worse than IGS orbit accuracy
    }

    SECTION("f64 Modified Julian Date spacing is ~0.63 us, i.e. ~4.7 mm at LEO") {
        const double mjd = 61000.0;
        const double ulp = std::nextafter(mjd, std::numeric_limits<double>::infinity()) - mjd;
        REQUIRE_THAT(ulp, WithinRel(std::ldexp(1.0, -37), 1e-12));

        const double micros = ulp * kSecondsPerDay * 1e6;
        REQUIRE_THAT(micros, WithinAbs(0.629, 0.005));
        // Equal to the whole 1 us budget before any arithmetic — also disqualified.
        REQUIRE(micros > 0.5);
    }

    SECTION("i64 seconds + f64 fraction resolves far below the budget") {
        // The chosen representation: the fraction lives in [0,1), where the
        // spacing is 2^-53 s.  SPEC-time §6 claims 0.11 fs.
        const double frac_ulp = std::nextafter(1.0, 0.0);
        const double spacing = 1.0 - frac_ulp;
        REQUIRE_THAT(spacing, WithinRel(std::ldexp(1.0, -53), 1e-12));
        REQUIRE(spacing * 1e15 < 0.2);            // sub-femtosecond
        REQUIRE(spacing * kLeoSpeed_m_s < 1e-12); // sub-picometre at LEO
    }

    SECTION("i64 seconds spans the supported range with room to spare") {
        constexpr double years = 9.223372036854775807e18 / kSecondsPerDay / 365.25;
        REQUIRE(years > 1e11);
    }
}

TEST_CASE("Catch2 covers the specs' tolerance vocabulary", "[toolchain][matchers]") {
    SECTION("WithinAbs expresses an absolute tolerance, e.g. '< 1 mm'") {
        const double a = 7000000.0;
        REQUIRE_THAT(a + 0.0009, WithinAbs(a, 0.001));
        REQUIRE_THAT(a + 0.0011, !WithinAbs(a, 0.001));
    }
    SECTION("WithinRel expresses a relative tolerance, e.g. 'relative 1e-6'") {
        const double a = 1.234e-9;
        REQUIRE_THAT(a * (1 + 9e-7), WithinRel(a, 1e-6));
        REQUIRE_THAT(a * (1 + 2e-6), !WithinRel(a, 1e-6));
    }
    SECTION("WithinULP expresses 'bit-comparable'") {
        const double a = 0.1 + 0.2;
        REQUIRE_THAT(a, WithinULP(0.30000000000000004, 0));
        REQUIRE_THAT(a, !WithinULP(0.3, 0));
    }
}
