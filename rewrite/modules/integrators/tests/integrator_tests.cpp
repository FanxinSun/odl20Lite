// integrator_tests.cpp — L3 step 2's gate.
//
// SPEC-integrators §8.  The two published checks establish DIFFERENT claims and
// are labelled as such: the order conditions prove the tableau is a valid RK7(8)
// pair; Fehlberg's Table XI proves it is HIS pair, because the accumulated
// errors depend on the actual coefficients and not only on the order to which
// they are correct.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/integrators/integrate.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using namespace odl::integrators;

namespace {

/// Fehlberg's Example (53):  y' = -2xy log z,  z' = 2xz log y
/// exact:  y = e^cos(x^2),  z = e^sin(x^2)
Vec<2> example53(double x, const Vec<2>& s) {
    return Vec<2>{-2.0 * x * s[0] * std::log(s[1]), 2.0 * x * s[1] * std::log(s[0])};
}
Vec<2> example53_exact(double x) {
    return Vec<2>{std::exp(std::cos(x * x)), std::exp(std::sin(x * x))};
}

constexpr double kMu = 3.986004415e14;   // m^3/s^2

Vec<6> two_body(double, const Vec<6>& s) {
    const double r = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    const double a = -kMu / (r * r * r);
    return Vec<6>{s[3], s[4], s[5], a * s[0], a * s[1], a * s[2]};
}

double norm6(const Vec<6>& a, const Vec<6>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < 3; ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(s);
}

}  // namespace

TEST_CASE("INTG-A-003  the weight columns and the printed truncation term agree",
          "[integrators][gate]") {
    // chat - c must be exactly (41/840) at positions 0, 10, 11, 12 with signs
    // (-, -, +, +) and zero elsewhere -- which is what (134) says. Two printed
    // objects from the same page checking each other.
    using namespace rkf78;
    for (int i = 0; i < kStages; ++i) {
        const double d = kCHat[static_cast<std::size_t>(i)] - kC[static_cast<std::size_t>(i)];
        const double want = (i == 0 || i == 10) ? -kErrorWeight
                          : (i == 11 || i == 12) ? +kErrorWeight : 0.0;
        INFO("stage " << i);
        CHECK_THAT(d, Catch::Matchers::WithinAbs(want, 1e-15));
    }
    CHECK(kOrder == 7);
    CHECK(kEstimatorOrder == 8);
    CHECK(kOrder8Violations == 40);   // Fehlberg's own prose count, Section XV
}

TEST_CASE("INTG-A-005  the estimator is EXACTLY zero on a quadrature problem",
          "[integrators][gate]") {
    // SPEC-integrators §3.4. alpha_0 = alpha_11 = 0 and alpha_10 = alpha_12 = 1,
    // so for a right-hand side depending on x alone the four evaluations of
    // (134) cancel in pairs. THE CONTROLLER IS BLIND HERE, NOT OPTIMISTIC.
    auto quad = [](double x, const Vec<1>&) { return Vec<1>{std::cos(x)}; };

    const double h = 0.75;                       // deliberately coarse
    const Vec<1> y0{0.0};
    const auto step = rkf78_step<1>(quad, 0.0, y0, h);

    // the estimate is not small; it is zero
    CHECK(step.error[0] == 0.0);

    // ...while the true error is not. integral of cos from 0 to h is sin(h).
    const double truth = std::sin(h);
    const double actual = std::abs(step.y[0] - truth);
    INFO("estimate " << step.error[0] << ", true error " << actual);
    CHECK(actual > 0.0);

    // AND THE CONSEQUENCE, exhibited: the controller accepts any step at any
    // tolerance, because nothing ever exceeds it.
    auto r = integrate<1>(quad, 0.0, y0, 10.0, 1e-30);
    REQUIRE(r.has_value());
    const double end_err = std::abs(r->y[0] - std::sin(10.0));
    INFO("tolerance 1e-30, steps " << r->record.accepted << ", rejections "
         << r->record.rejected << ", worst estimate " << r->record.worst_estimate
         << ", TRUE error " << end_err);
    CHECK(r->record.rejected == 0);              // nothing was ever rejected
    CHECK(r->record.worst_estimate == 0.0);      // nothing was ever estimated
    CHECK(end_err > 1e-30);                      // and the answer is wrong by far more
}

TEST_CASE("INTG-A-004  Example (53) against its closed form, and against Table XI",
          "[integrators][gate]") {
    const Vec<2> y0 = example53_exact(0.0);
    REQUIRE_THAT(y0[0], Catch::Matchers::WithinRel(std::numbers::e, 1e-15));
    REQUIRE(y0[1] == 1.0);

    auto r = integrate<2>(example53, 0.0, y0, 5.0, 1e-18);
    REQUIRE(r.has_value());
    const Vec<2> truth = example53_exact(5.0);
    const double dy = r->y[0] - truth[0];
    const double dz = r->y[1] - truth[1];

    INFO("steps " << r->record.accepted << " (Fehlberg 818), rejections "
         << r->record.rejected << ", evaluations " << r->record.evaluations
         << " (Fehlberg 10634)\n  dy = " << dy << "  (Fehlberg -0.2509e-13)"
         << "\n  dz = " << dz << "  (Fehlberg -0.5135e-13)");

    // INTG-P-2, PRE-REGISTERED. The gate is the ORDER OF MAGNITUDE, within a
    // factor of ten. Four significant figures are a 1968 machine's accumulated
    // arithmetic in a different operation order over a different step sequence,
    // and a four-figure match would be evidence that something is wrong.
    CHECK(std::abs(dy) > 1e-16);
    CHECK(std::abs(dy) < 1e-12);
    CHECK(std::abs(dz) > 1e-16);
    CHECK(std::abs(dz) < 1e-12);
    // the step count is REPORTED, not gated: Fehlberg describes the step-size
    // control in prose, so a different safety factor gives a different sequence.
    CHECK(r->record.accepted > 100);
}

TEST_CASE("INTG-A-006  two-body, and step-size insensitivity demonstrated",
          "[integrators][gate]") {
    const double R = 7331.0e3;
    const double v = std::sqrt(kMu / R);
    const double period = 2.0 * std::numbers::pi * std::sqrt(R * R * R / kMu);
    const Vec<6> s0{R, 0, 0, 0, v, 0};

    // INSENSITIVITY: the answer must not depend on how it was stepped.
    std::vector<double> tols{1e-8, 1e-10, 1e-12};
    std::vector<double> errs;
    for (double tol : tols) {
        auto r = integrate<6>(two_body, 0.0, s0, period, tol);
        REQUIRE(r.has_value());
        errs.push_back(norm6(r->y, s0));
        INFO("tol " << tol << ": steps " << r->record.accepted << " rejections "
             << r->record.rejected << " closure " << errs.back() << " m");
    }
    // tightening by 1e4 must not make the answer worse, and each closure is far
    // below the orbit's scale -- the two statements insensitivity actually means
    for (double e : errs) CHECK(e < 1.0e-3);
    CHECK(errs.back() <= errs.front() * 10.0);

    // ORDER, recovered from the slope rather than asserted. Fixed steps, and
    // COARSE ONES: at period/200 a 7th-order method on this orbit is already
    // round-off limited -- about 2e-7 m of accumulated arithmetic against a
    // truncation term near 5e-10 m -- so halving the step there measures the
    // round-off walk and not the order. Measured first, then chosen.
    const double h0 = period / 20.0;
    std::vector<double> he;
    for (double h : {h0, h0 / 2.0}) {
        Vec<6> y = s0;
        double t = 0.0;
        const auto n = static_cast<std::size_t>(std::llround(period / h));
        for (std::size_t i = 0; i < n; ++i) { y = rkf78_step<6>(two_body, t, y, h).y; t += h; }
        he.push_back(norm6(y, s0));
    }
    const double slope = std::log2(he[0] / he[1]);
    INFO("fixed-step closures " << he[0] << " and " << he[1] << " -> slope " << slope);
    CHECK(slope > 6.0);            // a 7th-order propagator, measured
}

TEST_CASE("INTG-A-007  RK4 converges at fourth order, measured", "[integrators]") {
    const double R = 7331.0e3;
    const double v = std::sqrt(kMu / R);
    const double period = 2.0 * std::numbers::pi * std::sqrt(R * R * R / kMu);
    const Vec<6> s0{R, 0, 0, 0, v, 0};
    std::vector<double> e;
    for (double n : {200.0, 400.0}) {
        const double h = period / n;
        Vec<6> y = s0;
        double t = 0.0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
            y = rk4_step<6>(two_body, t, y, h); t += h;
        }
        e.push_back(norm6(y, s0));
    }
    const double slope = std::log2(e[0] / e[1]);
    INFO("RK4 closures " << e[0] << " and " << e[1] << " -> slope " << slope);
    CHECK(slope > 3.5);
    CHECK(slope < 4.5);
}

TEST_CASE("INTG-A-011  the controller's constants change the COST and not the ANSWER",
          "[integrators][gate]") {
    // INTG-Q-001. The safety factor and the growth and shrink bounds are this
    // tree's choices, not Fehlberg's -- he gives the PROCEDURE in prose. Naming
    // them is a disclosure; this test is what BOUNDS it. They may change how
    // much work the integration does and must not be able to reach the result.
    //
    // THE BOUND IS MEASURED, NOT ASSERTED, AND BY A ROUTE IMMUNE TO THE EFFECT
    // BEING TESTED. An absolute floor would have to come from somewhere, and
    // any number chosen after seeing the separation is a tolerance fitted to its
    // own result. So the band is established from runs that differ ONLY in the
    // initial step -- same controller constants throughout, so controller
    // participation is impossible by construction, and what remains is the
    // arithmetic of a different step sequence. The claim under test is then
    // RELATIVE: changing the constants must not separate the answer by more
    // than merely changing where the sequence starts already does.
    const double R = 7331.0e3;
    const double v = std::sqrt(kMu / R);
    const double period = 2.0 * std::numbers::pi * std::sqrt(R * R * R / kMu);
    const Vec<6> s0{R, 0, 0, 0, v, 0};
    const double tol = 1e-10;

    const Control a{0.9, 5.0, 0.1, 20};
    auto ra = integrate<6>(two_body, 0.0, s0, period, tol, a);
    REQUIRE(ra.has_value());

    // the band: same controller, different starting step
    double band = 0.0;
    for (double frac : {150.0, 200.0, 300.0, 500.0}) {
        auto rc = integrate<6>(two_body, 0.0, s0, period, tol, a, period / frac);
        REQUIRE(rc.has_value());
        band = std::max(band, norm6(ra->y, rc->y));
    }

    // the claim: all four constants changed at once
    const Control b{0.75, 2.0, 0.25, 20};
    auto rb = integrate<6>(two_body, 0.0, s0, period, tol, b);
    REQUIRE(rb.has_value());
    const double separation = norm6(ra->y, rb->y);

    INFO("A " << ra->record.accepted << " steps, B " << rb->record.accepted << " steps\n"
         "  separation from changing all four constants: " << separation << " m\n"
         "  band from changing only the initial step   : " << band << " m");

    // THE COST MOVED...
    CHECK(ra->record.accepted != rb->record.accepted);
    // ...AND THE ANSWER DID NOT: the constants reach no further into the result
    // than a step-sequence shift that cannot involve them at all.
    CHECK(separation <= band);
    // and the whole band is far below anything that could matter dynamically
    CHECK(band < 1.0e-5);
}

TEST_CASE("INTG-A-008  refusals fire, and do not fire on the adjacent input", "[integrators]") {
    auto f = [](double, const Vec<1>& y) { return Vec<1>{y[0]}; };
    const Vec<1> y0{1.0};

    const auto bad = integrate<1>(f, 0.0, y0, 1.0, -1.0);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "INTG-F-005");

    const auto zero = integrate<1>(f, 0.0, y0, 1.0, 0.0);
    REQUIRE_FALSE(zero.has_value());
    CHECK(zero.error().id == "INTG-F-005");

    // proven both ways
    CHECK(integrate<1>(f, 0.0, y0, 1.0, 1e-10).has_value());

    // INTG-F-004, fired by a REAL condition rather than a constructed one: an
    // oversized first step on Example (53) drives z negative, and log(z) is not
    // finite. The refusal names the component and the abscissa.
    const Vec<2> e0 = example53_exact(0.0);
    const auto nan = integrate<2>(example53, 0.0, e0, 5.0, 1e-18, Control{}, 2.0);
    REQUIRE_FALSE(nan.has_value());
    CHECK(nan.error().id == "INTG-F-004");
    // and the same problem from a sane first step does not refuse
    CHECK(integrate<2>(example53, 0.0, e0, 5.0, 1e-18).has_value());
}

TEST_CASE("INTG-A-009  accepted and rejected steps are counted separately", "[integrators]") {
    // Two-body, which has no domain restriction, with a deliberately oversized
    // first step so that rejections certainly occur. Example (53) cannot be used
    // here: an oversized step there takes z negative and the integration refuses
    // rather than rejecting, which INTG-A-008 exercises instead.
    const double R = 7331.0e3;
    const double v = std::sqrt(kMu / R);
    const double period = 2.0 * std::numbers::pi * std::sqrt(R * R * R / kMu);
    const Vec<6> s0{R, 0, 0, 0, v, 0};
    auto r = integrate<6>(two_body, 0.0, s0, period, 1e-10, Control{}, period / 2.0);
    REQUIRE(r.has_value());
    INFO("accepted " << r->record.accepted << " rejected " << r->record.rejected
         << " evaluations " << r->record.evaluations);
    CHECK(r->record.evaluations ==
          (r->record.accepted + r->record.rejected) * static_cast<std::size_t>(rkf78::kStages));
    CHECK(r->record.rejected > 0);   // the oversized initial step must be rejected
}
