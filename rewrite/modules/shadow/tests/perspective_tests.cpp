// perspective_tests.cpp — SPEC-shadow §4, the PPM.
//
// THE COMPARATOR HERE IS THE DEFINITION, not the other model.  Fs is the
// fraction of the solar disc's SOLID ANGLE from which the ray to the satellite
// is not stopped by the Earth, and that can be computed without any projection
// at all: a direction w is blocked iff r + p w meets the ellipsoid at some p > 0.
// Both `conical` and `perspective` are approximations OF this quantity, by
// different routes, so neither can be the other's oracle -- plan §4 rule 2, and
// the reason this file does not simply diff the two models.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/shadow/conical.hpp>
#include <odl/shadow/perspective.hpp>

#include <cmath>
#include <numbers>
#include <tuple>
#include <vector>

using namespace odl;
using namespace odl::shadow;

namespace {

constexpr double kAu = 1.495978707e11;
constexpr double kSphere = kEarthRadiusM;

Vec3 sun_at() { return Vec3{kAu, 0.0, 0.0}; }
/// Geocentric angle `g` from the anti-Sun direction, in the equatorial plane.
Vec3 sat_at(double r, double g) { return Vec3{-r * std::cos(g), r * std::sin(g), 0.0}; }
/// The same, over the pole, where an oblate Earth presents its short axis.
Vec3 sat_polar(double r, double g) { return Vec3{-r * std::cos(g), 0.0, r * std::sin(g)}; }
double terminator(double r) { return std::asin(kSphere / r); }

/// Fs from the definition: exact in the radial direction (each blocked interval's
/// ends found by bisection, the radial integral done in closed form as
/// cos a1 - cos a2), discretised only in azimuth.  A cell count would converge
/// like 1/n and could not tell the two models apart; this one can.
/// The Sun's radial brightness, as the cumulative weight out to angle `a`.
/// u = 0 is the UNIFORM disc -- solid angle, which is what both models assume.
/// u = 3/5 is the Eddington grey atmosphere's BOLOMETRIC limb darkening,
/// I(mu)/I(1) = (2 + 3 mu)/5, which is the law SRP actually wants: the quantity
/// is total radiant flux, not a visible band.  Closed form, from
/// INTEGRAL sqrt(c^2 - k^2) dc, and checked against its own disc mean 1 - u/3.
struct Sky {
    double a_s, S, k, u;
    Sky(double as, double uu) : a_s(as), S(std::sin(as)), k(std::cos(as)), u(uu) {}
    [[nodiscard]] double F(double c) const {
        const double q = std::sqrt(std::max(0.0, (c - k) * (c + k)));   // factored: c^2 - k^2 cancels
        return 0.5 * c * q - 0.5 * k * k * std::log(c + q);
    }
    [[nodiscard]] double W(double a) const {
        const double c = std::cos(a);
        if (u == 0.0) return 1.0 - c;
        return (1.0 - u) * (1.0 - c) + (u / S) * (F(1.0) - F(c));
    }
};

double by_solid_angle(const Vec3& sun, const Vec3& r, double a, double b, int nphi = 8000,
                      double u = 0.0) {
    const Vec3 ts{sun.x - r.x, sun.y - r.y, sun.z - r.z};
    const double ds = ts.norm();
    const Vec3 c{ts.x / ds, ts.y / ds, ts.z / ds};
    const Sky sky(std::asin(kSunRadiusM / ds), u);
    const double as = sky.a_s;
    const Vec3 seed = (std::abs(c.x) <= std::abs(c.y) && std::abs(c.x) <= std::abs(c.z))
                          ? Vec3{1, 0, 0}
                          : (std::abs(c.y) <= std::abs(c.z) ? Vec3{0, 1, 0} : Vec3{0, 0, 1});
    Vec3 e1 = seed.cross(c);
    const double n1 = e1.norm();
    e1 = Vec3{e1.x / n1, e1.y / n1, e1.z / n1};
    const Vec3 e2 = c.cross(e1);
    const double ia2 = 1.0 / (a * a), ib2 = 1.0 / (b * b);
    const auto A = [&](const Vec3& w) { return Vec3{ia2 * w.x, ia2 * w.y, ib2 * w.z}; };
    const Vec3 Ar = A(r);
    const double rAr = r.dot(Ar);
    const auto hit = [&](double al, double ph) {
        const double ca = std::cos(al), sa = std::sin(al), cp = std::cos(ph), sp = std::sin(ph);
        const Vec3 w{ca * c.x + sa * (cp * e1.x + sp * e2.x), ca * c.y + sa * (cp * e1.y + sp * e2.y),
                     ca * c.z + sa * (cp * e1.z + sp * e2.z)};
        const double wAw = w.dot(A(w)), wAr = w.dot(Ar);
        return wAr * wAr - wAw * (rAr - 1.0) >= 0.0 && wAr < 0.0;
    };
    constexpr int kRad = 120;
    double blocked = 0.0;
    for (int j = 0; j < nphi; ++j) {
        const double ph = 2.0 * std::numbers::pi * (j + 0.5) / nphi;
        std::vector<double> edge;
        bool prev = hit(0.0, ph);
        const bool first = prev;
        for (int i = 1; i <= kRad; ++i) {
            const double al = as * i / kRad;
            if (hit(al, ph) != prev) {
                double lo = as * (i - 1) / kRad, hi = al;
                const bool at_lo = hit(lo, ph);
                for (int k = 0; k < 70; ++k) {
                    const double m = 0.5 * (lo + hi);
                    (hit(m, ph) == at_lo) ? lo = m : hi = m;
                }
                edge.push_back(0.5 * (lo + hi));
                prev = !prev;
            }
        }
        double acc = 0.0, a0 = 0.0;
        bool st = first;
        for (double ee : edge) {
            if (st) acc += sky.W(ee) - sky.W(a0);
            st = !st;
            a0 = ee;
        }
        if (st) acc += sky.W(as) - sky.W(a0);
        blocked += acc;
    }
    return 1.0 - (blocked / nphi) / sky.W(as);
}

PerspectiveResult must_run(const Vec3& sun, const Vec3& sat, double a, double b,
                           Atmosphere atm = Atmosphere::none, double gamma = 1.0) {
    auto p = detail::perspective_on(sun, sat, a, b, atm, gamma);
    REQUIRE(p.has_value());
    return *p;
}

/// SHDW-R-021, asserted rather than described: the PPM's interface admits an
/// EARTH-FIXED position and nothing else.  A = diag(a^-2, a^-2, b^-2) is the
/// Earth only in a frame that turns with it, so a GCRS position must not compile.
/// Both directions are checked, because a concept that is false for every frame
/// would satisfy the negative half on its own (plan §4 rule 5).
template <frames::Frame F>
concept ppm_accepts = requires(frames::Position<F> p) { shadow::perspective(p, p); };

static_assert(ppm_accepts<frames::Frame::ITRS>,
              "SHDW-R-021: the PPM must accept an Earth-fixed position");
static_assert(!ppm_accepts<frames::Frame::GCRS>,
              "SHDW-R-021: the PPM must not accept GCRS -- the ellipsoid is not a body there");
static_assert(!ppm_accepts<frames::Frame::TIRS>, "SHDW-R-021");
static_assert(!ppm_accepts<frames::Frame::TEME>, "SHDW-R-021");
static_assert(!ppm_accepts<frames::Frame::BCRS>, "SHDW-R-021");

/// And the converse for the conical model, whose sphere IS a sphere in every
/// frame: it takes GCRS and not ITRS, so the two cannot be swapped by accident.
template <frames::Frame F>
concept secm_accepts = requires(frames::Position<F> p) { shadow::conical(p, p); };
static_assert(secm_accepts<frames::Frame::GCRS>, "the SECM takes GCRS");
static_assert(!secm_accepts<frames::Frame::ITRS>, "and the two signatures do not overlap");

}  // namespace

TEST_CASE("SHDW-A-008  the PPM against the definition, on both silhouettes", "[shadow][gate]") {
    // LEO gives a HYPERBOLIC silhouette and GEO an elliptical one; the kinds are
    // asserted so that a run which exercised only one of them cannot pass.
    struct Case { const char* name; double r; Silhouette expect; };
    const Case cases[] = {{"LEO  (r = 7331 km)", 7331.0e3, Silhouette::hyperbola},
                          {"GPS  (r = 26560 km)", 26560.0e3, Silhouette::ellipse},
                          {"GEO  (r = 42164 km)", 42164.0e3, Silhouette::ellipse}};
    double worst = 0.0;
    int checked = 0, hyperbolae = 0, ellipses = 0;
    for (const auto& c : cases) {
        for (int i = -1; i <= 1; ++i) {
            const double g = terminator(c.r) + i * 0.0025;
            const Vec3 sat = sat_at(c.r, g), sun = sun_at();
            const auto p = must_run(sun, sat, kSphere, kSphere);
            CHECK(p.silhouette == c.expect);
            p.silhouette == Silhouette::hyperbola ? ++hyperbolae : ++ellipses;
            const double truth = by_solid_angle(sun, sat, kSphere, kSphere);
            INFO(c.name << " at g = " << g << ": PPM " << p.fraction << ", definition " << truth);
            CHECK(p.fraction > 0.0);
            CHECK(p.fraction < 1.0);              // every case is genuinely penumbral
            worst = std::max(worst, std::abs(p.fraction - truth));
            ++checked;
        }
    }
    INFO("checked " << checked << " penumbral geometries (" << hyperbolae << " hyperbolic, "
         << ellipses << " elliptical); worst |PPM - definition| = " << worst);
    CHECK(checked == 9);
    CHECK(hyperbolae == 3);
    CHECK(ellipses == 6);
    // 1e-5 is the comparator's own convergence at nphi = 8000, measured by
    // refinement (500..16000 gave 0.494081, 0.494496, 0.494394, 0.494449,
    // 0.4944448, 0.4944436), NOT a bound chosen to let the model through: the
    // conical model misses the same quantity by 1.9e-4 at LEO and would fail it.
    CHECK(worst < 1.0e-5);
}

TEST_CASE("SHDW-A-009  the SECM's departure from the definition, against the effect the PPM is for",
          "[shadow][gate]") {
    // Plan §4 rule 3: the quantities compared are measured in ONE place at ONE
    // reference point, because the comparison is the whole content of the test.
    //
    // "Conical" being a family is SPEC-shadow §3.  This is the sixth choice, and
    // the paper does not name it: whether the occulted-area ratio is taken on the
    // FLAT SKY, with angular radii used as planar lengths, or in the true solid
    // angle.  The SECM does the first.  The PPM does the second, exactly, because
    // a perspective projection maps the straight lines of the occultation to
    // straight lines.  At LEO that unnamed choice is worth a HUNDRED TIMES the
    // oblateness the PPM is adopted for.
    struct Row { const char* name; double r; };
    const Row rows[] = {{"LEO", 7331.0e3}, {"GPS", 26560.0e3}, {"GEO", 42164.0e3}};
    double leo_conical_error = 0.0, leo_oblateness = 0.0;
    int measured = 0;
    std::string table;
    for (const auto& row : rows) {
        const double g = terminator(row.r);
        const Vec3 sat = sat_at(row.r, g), sun = sun_at();
        const auto c = conical(frames::Position<frames::Frame::GCRS>{sun},
                               frames::Position<frames::Frame::GCRS>{sat});
        REQUIRE(c.has_value());
        const auto sphere = must_run(sun, sat, kSphere, kSphere);
        const auto wgs84 = must_run(sun, sat, kEarthEquatorialRadiusM, kEarthPolarRadiusM);
        const double truth = by_solid_angle(sun, sat, kSphere, kSphere);
        const double conical_error = std::abs(c->fraction - truth);
        const double ppm_error = std::abs(sphere.fraction - truth);
        const double oblateness = std::abs(wgs84.fraction - sphere.fraction);
        table += std::string(row.name) + ": |SECM - definition| " + std::to_string(conical_error)
               + ", |PPM - definition| " + std::to_string(ppm_error)
               + ", oblateness |PPM(WGS84) - PPM(sphere)| " + std::to_string(oblateness) + "\n";
        if (std::string(row.name) == "LEO") { leo_conical_error = conical_error; leo_oblateness = oblateness; }
        // whichever orbit, the PPM is nearer the definition than the SECM is
        CHECK(ppm_error < conical_error);
        // AND THE POLAR RADIUS REACHES THE ARITHMETIC.  Without this the whole
        // test passes on a model that silently ignored `b`.
        CHECK(oblateness > 0.0);
        ++measured;
    }
    INFO("at the terminator of each orbit:\n" << table);
    CHECK(measured == 3);
    INFO("at LEO the SECM's flat-sky departure is " << leo_conical_error / leo_oblateness
         << "x the oblateness effect");
    CHECK(leo_conical_error / leo_oblateness > 50.0);
}

TEST_CASE("SHDW-A-010  the image plane's distance cancels", "[shadow][gate]") {
    // gamma is a scale on a plane whose areas are both quadratic in it, so Fs
    // must not move.  It is an argument of `perspective_on` for no other reason
    // than to be varied here.
    const double r = 7331.0e3;
    const Vec3 sat = sat_at(r, terminator(r)), sun = sun_at();
    const double reference = must_run(sun, sat, kSphere, kSphere, Atmosphere::none, 1.0).fraction;
    double worst = 0.0;
    int varied = 0;
    for (double gamma : {1.0e-3, 1.0, 1.0e3, 1.0e6}) {
        const double f = must_run(sun, sat, kSphere, kSphere, Atmosphere::none, gamma).fraction;
        worst = std::max(worst, std::abs(f - reference));
        ++varied;
    }
    INFO("Fs = " << reference << " over " << varied << " image-plane distances spanning 1e9; "
         "worst spread " << worst);
    CHECK(varied == 4);
    CHECK(reference > 0.0);
    CHECK(reference < 1.0);
    CHECK(worst < 1.0e-12);
}

TEST_CASE("SHDW-A-011  oblateness is largest over the pole, which is where it should be",
          "[shadow][gate]") {
    // A sign check on the physics rather than on the arithmetic: an oblate Earth
    // presents a SHORTER limb over the pole, so it blocks less there than a
    // sphere of equatorial radius does, and Fs must come out HIGHER.
    const double r = 7331.0e3;
    const Vec3 sun = sun_at();
    const Vec3 over_pole = sat_polar(r, terminator(r));
    const auto sphere = must_run(sun, over_pole, kSphere, kSphere);
    const auto wgs84 = must_run(sun, over_pole, kEarthEquatorialRadiusM, kEarthPolarRadiusM);
    INFO("over the pole: PPM(sphere) " << sphere.fraction << ", PPM(WGS84) " << wgs84.fraction
         << ", difference " << wgs84.fraction - sphere.fraction);
    CHECK(wgs84.fraction > sphere.fraction);
    CHECK(wgs84.fraction - sphere.fraction > 1.0e-4);   // and it is not a rounding difference
}

TEST_CASE("SHDW-A-012  the atmosphere only ever dims, and widens the penumbra", "[shadow][gate]") {
    const double r = 7331.0e3;                           // below 1983 km altitude: see SHDW-A-013
    const Vec3 sun = sun_at();
    int samples = 0, dimmer = 0, atm_penumbral = 0, bare_penumbral = 0;
    for (int i = -40; i <= 40; ++i) {
        const double g = terminator(r) + i * 0.0006;
        const Vec3 sat = sat_at(r, g);
        const auto bare = must_run(sun, sat, kEarthEquatorialRadiusM, kEarthPolarRadiusM);
        const auto atm = must_run(sun, sat, kEarthEquatorialRadiusM, kEarthPolarRadiusM,
                                  Atmosphere::linear_toa);
        CHECK(atm.fraction <= bare.fraction + 1e-12);    // an atmosphere cannot brighten
        if (atm.fraction < bare.fraction - 1e-12) ++dimmer;
        if (atm.state == State::penumbra) ++atm_penumbral;
        if (bare.state == State::penumbra) ++bare_penumbral;
        ++samples;
    }
    INFO("over " << samples << " geometries: " << dimmer << " strictly dimmer; penumbral samples "
         << bare_penumbral << " without the atmosphere, " << atm_penumbral << " with it");
    CHECK(samples == 81);
    CHECK(dimmer > 0);                                   // the option is doing something
    CHECK(atm_penumbral > bare_penumbral);               // and the right thing
}

TEST_CASE("SHDW-A-013  the refusals fire, from the place they will fire from", "[shadow][gate]") {
    const Vec3 sun = sun_at();

    // Inside the Earth: there is no silhouette to project.
    auto inside = detail::perspective_on(sun, Vec3{1.0e6, 0.0, 0.0}, kEarthEquatorialRadiusM,
                                         kEarthPolarRadiusM, Atmosphere::none, 1.0);
    REQUIRE_FALSE(inside.has_value());
    CHECK(inside.error().id == "SHDW-F-004");

    // ABOVE 1983 km ALTITUDE LI19's FIVE ATMOSPHERIC CASES DO NOT EXHAUST THE
    // GEOMETRY.  Fig. 8's cases b, c and d each have the solar disc meeting at
    // most two of {solid Earth, atmosphere, clear sky}; eq 45 in particular has a
    // term for the blocked area and one for the in-atmosphere area and none for a
    // fully lit one.  That is complete exactly while the atmosphere's image is at
    // least as thick as the solar disc is wide --
    //
    //     asin((Re + 50 km)/r) - asin(Re/r)  >=  2 asin(Rs/d)
    //
    // -- which holds to r = 8361 km and fails above it.  GRACE, at 500 km, is
    // inside it; Galileo, at 29 600 km, is not, and the paper validates against
    // both.  This refuses rather than invent a sixth case, because inventing one
    // is a change to the model and not a change to this implementation of it.
    const double r = 26560.0e3;
    // WIDE ENOUGH TO LEAVE THE STRADDLE on both sides: a window that saw only
    // refusals would satisfy "the refusal fires" while proving nothing about when
    // it does not, which is the half that says it is a boundary and not a wall.
    int refusals = 0, penumbral = 0, lit = 0, dark = 0;
    for (int i = -20; i <= 20; ++i) {
        const double g = terminator(r) + i * 0.0012;
        auto q = detail::perspective_on(sun, sat_at(r, g), kEarthEquatorialRadiusM,
                                        kEarthPolarRadiusM, Atmosphere::linear_toa, 1.0);
        if (!q) { CHECK(q.error().id == "SHDW-F-009"); ++refusals; continue; }
        if (q->state == State::penumbra) ++penumbral;
        if (q->state == State::sunlight) ++lit;
        if (q->state == State::umbra) ++dark;
    }
    INFO("at GPS altitude, " << refusals << " of 41 atmospheric geometries refused as outside "
         "LI19 Fig. 8's five cases; the rest resolved as " << dark << " umbra, " << penumbral
         << " penumbra, " << lit << " sunlight");
    CHECK(refusals > 0);
    CHECK(refusals < 41);       // and it is not refusing everything
    CHECK(lit > 0);
    CHECK(dark > 0);
}

TEST_CASE("SHDW-A-014  the parabola refusal fires at the place it exists for", "[shadow][gate]") {
    // Plan §4 rule 5: a guard is proven by MAKING IT FIRE, in the place it will
    // fire from.  The parabola is the boundary between the two silhouettes, and
    // for a sphere it is |cos psi| = Re/r exactly, so it can be bisected onto
    // rather than argued about.  Either side of it the model must still answer --
    // a guard that fires on a whole neighbourhood is a wall, not a boundary.
    const double r = 7331.0e3;
    const Vec3 sun = sun_at();
    const auto kind_at = [&](double g) -> int {          // -1 refused, 0 ellipse, 1 hyperbola
        auto p = detail::perspective_on(sun, sat_at(r, g), kSphere, kSphere, Atmosphere::none, 1.0);
        if (!p) return -1;
        return p->silhouette == Silhouette::hyperbola ? 1 : 0;
    };
    // psi is the angle at the satellite between the Earth's centre and the Sun;
    // in this geometry it tracks the geocentric angle monotonically, so bracket
    // in g and let the model report which side it is on.
    double lo = 0.05, hi = 1.4;                          // ellipse ... hyperbola
    REQUIRE(kind_at(lo) == 0);
    REQUIRE(kind_at(hi) == 1);
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        const int k = kind_at(mid);
        if (k == -1) { lo = mid; break; }                 // landed on the refusal
        (k == 0) ? lo = mid : hi = mid;
    }
    const double boundary = 0.5 * (lo + hi);
    auto at_boundary = detail::perspective_on(sun, sat_at(r, boundary), kSphere, kSphere,
                                              Atmosphere::none, 1.0);
    INFO("the ellipse/hyperbola boundary is at geocentric angle " << boundary
         << " rad; asin(Re/r) puts the terminator at " << terminator(r));
    // Whether the bisection lands exactly ON the degenerate point or a rounding
    // step to one side of it, BOTH are correct answers; what is asserted is that
    // the two sides are resolved and differ.
    if (!at_boundary) CHECK(at_boundary.error().id == "SHDW-F-005");
    REQUIRE(kind_at(boundary - 1e-6) == 0);
    REQUIRE(kind_at(boundary + 1e-6) == 1);
    CHECK(boundary > 0.05);
    CHECK(boundary < 1.4);
}

TEST_CASE("SHDW-A-015  a non-positive ellipsoid radius or image plane is refused", "[shadow][gate]") {
    const Vec3 sun = sun_at(), sat = sat_at(7331.0e3, terminator(7331.0e3));
    for (auto [a, b, gamma] : {std::tuple{-1.0, kSphere, 1.0}, std::tuple{kSphere, 0.0, 1.0},
                               std::tuple{kSphere, kSphere, 0.0}}) {
        auto p = detail::perspective_on(sun, sat, a, b, Atmosphere::none, gamma);
        REQUIRE_FALSE(p.has_value());
        CHECK(p.error().id == "SHDW-F-003");
    }
    // and it does NOT fire on the adjacent accepted input
    CHECK(detail::perspective_on(sun, sat, kSphere, kSphere, Atmosphere::none, 1.0).has_value());
}

TEST_CASE("SHDW-A-016  the seventh axis: the Sun's brightness profile dominates both the others",
          "[shadow][gate]") {
    // THE REFERENCE DEFINITION IS ITSELF A FAMILY CHOICE, and measuring it is the
    // only way to know what `SHDW-A-008`'s agreement figure means. Fs as an
    // occulted-AREA ratio assumes a UNIFORMLY BRIGHT solar disc; the Sun is limb
    // darkened, and for SRP the law wanted is BOLOMETRIC -- Eddington's grey
    // atmosphere gives I(mu)/I(1) = (2 + 3 mu)/5, the linear law with u = 3/5
    // exactly. Both `conical` and `perspective` assume the uniform disc, so this
    // affects NEITHER of them relative to the other; what it affects is the claim
    // that either agrees with "the definition".
    const double r = 7331.0e3;
    const Vec3 sun = sun_at();

    // the comparator's own self-check first: the disc-integrated mean intensity
    // of the linear law is 1 - u/3 in the flat-disc limit, a closed form that
    // owes nothing to the ray casting.
    {
        const Sky sky(std::asin(kSunRadiusM / kAu), 0.6);
        const double mean = sky.W(sky.a_s) / (1.0 - std::cos(sky.a_s));
        INFO("disc mean / centre = " << mean << ", closed form 1 - u/3 = " << 1.0 - 0.6 / 3.0);
        CHECK_THAT(mean, Catch::Matchers::WithinAbs(1.0 - 0.6 / 3.0, 1.0e-5));
    }

    double peak_limb = 0.0, peak_proj = 0.0, peak_obl = 0.0;
    int penumbral = 0;
    for (int i = 0; i <= 40; ++i) {
        const double g = terminator(r) - 0.006 + 0.012 * i / 40.0;
        const Vec3 sat = sat_at(r, g);
        const double uniform = by_solid_angle(sun, sat, kSphere, kSphere, 2000, 0.0);
        if (!(uniform > 1e-9 && uniform < 1.0 - 1e-9)) continue;   // penumbra only
        ++penumbral;
        const double limb = by_solid_angle(sun, sat, kSphere, kSphere, 2000, 0.6);
        const auto c = conical(frames::Position<frames::Frame::GCRS>{sun},
                               frames::Position<frames::Frame::GCRS>{sat});
        REQUIRE(c.has_value());
        const auto sphere = must_run(sun, sat, kSphere, kSphere);
        const auto wgs84 = must_run(sun, sat, kEarthEquatorialRadiusM, kEarthPolarRadiusM);
        peak_limb = std::max(peak_limb, std::abs(limb - uniform));
        peak_proj = std::max(peak_proj, std::abs(c->fraction - uniform));
        peak_obl = std::max(peak_obl, std::abs(wgs84.fraction - sphere.fraction));
    }
    INFO("over " << penumbral << " penumbral geometries at LEO, peak effect on Fs:\n"
         "    the Sun's brightness profile (u = 3/5, bolometric)   " << peak_limb << "\n"
         "    where the ratio is taken (flat sky vs projection)    " << peak_proj << "\n"
         "    the Earth's figure (sphere vs WGS 84)                " << peak_obl << "\n"
         "  ratios: brightness / projection = " << peak_limb / peak_proj
         << ", projection / figure = " << peak_proj / peak_obl);
    CHECK(penumbral > 20);                       // the sweep examined a penumbra
    CHECK(peak_limb > 1.0e-2);
    CHECK(peak_limb / peak_proj > 50.0);         // brightness dominates projection
    CHECK(peak_proj / peak_obl > 50.0);          // projection dominates figure
}
