// measmod_reference.cpp — reference values for SPEC-measmod.md's acceptance tests, from the DEFINITION.
//
// Plan L0 step 8, group C8, ported to C++ (the user's directive of 2026-10-06) from tools/measmod_reference.py (deleted by the same commit: `git show bc7cd6d:rewrite/tools/measmod_reference.py`), on the devkit's Decimal (group C7, with ln, exp and Decimal(float) since this group).
//
// Each section computes what a measmod test compares against, by a route that does NOT go through the code under test (the form of tools/legendre_reference.cpp): the definition evaluated independently, in
// 60-digit decimal arithmetic, and written into a header the tests include.  `--check` regenerates the header and fails if the committed one differs, so a reference value cannot drift away from its generator.
//
// Sections (each added by the step that needs it; the spec names the row that uses it):
//   registry   MEAS-A-010   Yarragadee (SOD 70900513) at the first normal point of the real January 2026 LAGEOS-1 file: the marker from x_ref + v (t - t_ref), the system reference point from the
//                           eccentricity, and the geodetic coordinates of both by an ITERATIVE geodetic solution (not ERFA's closed form).
//   shapiro    MEAS-A-025/026  the Shapiro term 2GM/c^2 ln[(r1+r2+rho)/(r1+r2-rho)] and its three partials, in 60-digit arithmetic at three geometries (LAGEOS-like zenith and 10 deg, a LEO case); the partials by
//                           the closed form AND by a 60-digit central difference, which must agree before either is emitted (the two-method guard).
//   vapour     MEAS-A-022   e = Rh * 6.11 * 10^(7.5 t / (237.3 + t)) (Marini & Murray 1973, eq. 22) at four (t, Rh), with 10^x as exp(x ln 10).
//   zenith     MEAS-A-020   the zenith delay of TN36 ch.9 eqs (9.3)-(9.7) at the IERS FCUL_ZD_HPA prolog's printed inputs, evaluated in 60 digits (the IMPLEMENTATION is compared with this; the PRINTED value is
//                           within 1 mm of it, 3.8 um away).
//   np         MEAS-A-039   the observed one-way-equivalent range c ToF/2 of the real first normal point (ToF 0.051212898595 s), in 60 digits.
//   lighttime  MEAS-A-030/031/041  the two-way light time with target AND station in uniform motion, for both epoch events, from the CLOSED FORM -- the two quadratics (c^2 - v^2) tau^2 -/+ 2 (D.v) tau - D^2 = 0
//                           solved in 60 digits, independently of any iteration -- at two geometries (LAGEOS-like and LEO-like), and the derivative of the range with respect to the target's position by a 60-digit
//                           central difference of that closed form (MEAS-A-041).
//   aberration MEAS-A-080/081  A(n; beta) and D(n; beta) of MEAS-R-053 at four (n, beta) cases (annual, oblique, large, diurnal), each formula written out and evaluated in 60 digits for the doubles the
//                           test passes, A(D(n)) = n checked in 60 digits before either is emitted.
//   emission   MEAS-A-088   the emission epoch and the geometric direction of a target in uniform motion seen from a fixed observer, from the closed form (c^2 - v^2) tau^2 + 2 (D.v) tau - D^2 = 0 in 60
//                           digits (the two geometries of the light-time tests).
//   position   MEAS-A-063   the first record of the vendored Horizons table (X, Y, Z printed in km) and of the real ILRS weekly SP3 (PL51, km), read as TEXT from the files and converted to metres in 60
//                           digits (exact decimal products) with their distances from the centre.
//
//   measmod_reference [--check] [--header PATH] [--root DIR]
//   exit 0 written / the committed header is what the generator writes   1 --check found a difference, or a guard of the generator refused   2 an argument error, or an input file that is missing, cannot be read or is not
//   the shape expected   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's product is a FILE, the header modules/measmod/tests/measmod_reference.hpp (10,813 bytes, 204 lines, four versions in git, sha256 b69ed31e...).  The port wrote it BYTE FOR
// BYTE, against a substitution list registered before the comparison and of ONE entry: line 1, the generator's own name and the command that verifies it.  (C8_proof_registration.txt, in this group's report
// files, holds the registration and the result.)  The values go through the C library's sin, cos and atan2 (geodetic) and through Decimal's ln, exp, sqrt and powers at 60 digits, and a double is what is printed.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.
//   * The Python died with a traceback on an input that is not the shape it reads (a line with too few fields, a number that is not one, no `$$SOE`, no `P` record, an `assert` of the shape of a file): this
//     REFUSES, naming what is wrong, exit 2.  Its three guards (the closed form against the central difference, A(D(n)) = n, the light-time condition) stay the tool's own and give exit 1 with the numbers.
//   * Text read back (`--check`) is compared as Python's read_text() hands it back: universal newlines.  A file that is not UTF-8, a directory, a path that cannot be written: REFUSED, exit 2, naming it.  The
//     argparse of the Python took any unambiguous abbreviation of an option; here the names are exact (`--opt=value` is taken), and `-h` prints this tool's own text.
//   * The date arithmetic of datetime.date(2026, 1, 1) - datetime.date(2015, 1, 1) is a function of its own (days_from_civil), tested against known dates.
//   * The regular expression of the Horizons record, X =\s*([-+0-9.E]+)\s+Y =\s*([-+0-9.E]+)\s+Z =\s*([-+0-9.E]+), is a scanner written for it: the engine's backtracking has one outcome here, because the token's
//     characters are no white space.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_reference.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>

namespace dk = odl::devkit;

namespace odl::tools::measmod_reference {

using dk::DecimalContext;
using dk::LocalContext;
using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "measmod_reference";

// an exact constant, as Decimal("...") makes it
Decimal D(std::string_view text) { return Decimal::from_string(text); }
Decimal from_double(double v) { return Decimal::from_double(v); }

const Decimal& wgs84_a() {   // WGS 84 (ERFA's n = 1, the ellipsoid the registry uses): a and 1/f, exactly as defined
    static const Decimal value = D("6378137");
    return value;
}

const Decimal& wgs84_inv_f() {
    static const Decimal value = D("298.257223563");
    return value;
}

const Decimal& speed_of_light() {
    static const Decimal value = Decimal(299792458);
    return value;
}

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    try {
        return dk::splitlines_py(dk::read_text(file));
    } catch (const std::runtime_error& exc) {
        throw MissingInput(exc.what());
    }
}

std::string read_whole(const std::filesystem::path& file) {
    try {
        return dk::read_text(file);
    } catch (const std::runtime_error& exc) {
        throw MissingInput(exc.what());
    }
}

bool starts_with(const std::string& s, std::string_view prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// the lines between "+NAME" and "-NAME", without those that begin with "*" (the Python's block())
std::vector<std::string> block(const std::vector<std::string>& lines, const std::string& name) {
    std::vector<std::string> out;
    bool on = false;
    for (const std::string& line : lines) {
        if (starts_with(line, "+" + name)) {
            on = true;
            continue;
        }
        if (starts_with(line, "-" + name)) break;
        if (on && !starts_with(line, "*")) out.push_back(line);
    }
    return out;
}

// a field of a line, or a refusal naming what was wanted (the Python's IndexError)
const std::string& field(const std::vector<std::string>& fields, std::size_t index, const std::string& what) {
    if (index >= fields.size()) throw MissingInput(what + ": a line has " + std::to_string(fields.size()) + " fields, field " + std::to_string(index) + " is wanted");
    return fields[index];
}

Decimal number(const std::string& token, const std::string& what) {
    try {
        return Decimal::from_python(token);
    } catch (const dk::DecimalError&) {
        throw MissingInput(what + ": '" + token + "' is not a number");
    }
}

Decimal dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec add_scaled(const Vec& a, const Vec& b, const Decimal& k) { return {a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k}; }
Vec to_vec(const std::array<double, 3>& v) { return {from_double(v[0]), from_double(v[1]), from_double(v[2])}; }

struct LtCase {
    const char* name;
    std::array<double, 3> s0, vs, r0, vr;
};

// the two geometries of the light-time tests, exactly representable inputs (doubles written as short decimals): station position and velocity, target position and velocity, all at the tag epoch, metres and
// metres per second, GCRS-like axes
const std::array<LtCase, 2>& lt_cases() {
    static const std::array<LtCase, 2> cases = {{{"lageos", {4.0e6, 3.1e6, 3.9e6}, {-250.0, 380.0, 0.0}, {9.0e6, 5.0e6, 6.0e6}, {-3000.0, 4500.0, 2800.0}},
                                                  {"leo", {6.0e6, -1.5e6, 1.9e6}, {180.0, 440.0, 0.0}, {7.0e6, -0.7e6, 2.4e6}, {2000.0, 6500.0, 3000.0}}}};
    return cases;
}

struct AbCase {
    const char* name;
    std::array<double, 3> n, beta;
};

// the aberration cases of MEAS-A-080/-081: a unit direction and the observer's velocity over c, exactly representable inputs (doubles written as short decimals)
const std::array<AbCase, 4>& ab_cases() {
    static const std::array<AbCase, 4> cases = {{{"annual", {0.6, 0.0, 0.8}, {0.0, 9.9e-5, 0.0}},                    // perpendicular: the annual aberration at its largest
                                                  {"oblique", {0.48, -0.6, 0.64}, {7.0e-5, -5.0e-5, 4.0e-5}},
                                                  {"large", {-0.36, 0.48, 0.8}, {3.0e-4, -4.0e-4, 5.0e-4}},          // |beta| = 7.07e-4: the upper end of the grid (1e-3)
                                                  {"diurnal", {0.0, 0.6, 0.8}, {0.0, 1.55e-6, 0.0}}}};              // the equatorial station's diurnal aberration
    return cases;
}

constexpr char kAxes[] = "xyz";

}  // namespace

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() { return Settings{default_root()}; }

std::int64_t days_from_civil(int year, int month, int day) {   // Howard Hinnant's days_from_civil: days since 1970-01-01 of the proleptic Gregorian calendar
    const std::int64_t y = year - (month <= 2 ? 1 : 0);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);                                                                                           // [0, 399]
    const unsigned doy = (153U * static_cast<unsigned>(month > 2 ? month - 3 : month + 9) + 2U) / 5U + static_cast<unsigned>(day) - 1U;                // [0, 365]
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;                                                                                   // [0, 146096]
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

const Decimal& pi() {
    static const Decimal value = D("3.14159265358979323846264338327950288419716939937510582097494459");
    return value;
}

const Decimal& ln10() {
    static const Decimal value = [] {
        LocalContext at_import(kPrecision);   // the Python computed it when the module was imported, with getcontext().prec = 60 set a line before
        return Decimal(10).ln();
    }();
    return value;
}

Decimal d_cos(const Decimal& x) {
    Decimal term(1);
    Decimal total(1);
    std::int64_t n = 0;
    const Decimal tiny = D("1e-70");
    while (term.abs() > tiny) {
        n += 2;
        term = -term * x * x / Decimal(n * (n - 1));
        total = total + term;
    }
    return total;
}

Decimal d_pow10(const Decimal& x) { return (x * ln10()).exp(); }

Geodetic geodetic(const Decimal& x, const Decimal& y, const Decimal& z) {
    const Decimal f = Decimal(1) / wgs84_inv_f();
    const Decimal e2 = f * (Decimal(2) - f);
    const Decimal p = (x * x + y * y).sqrt();
    const double lon = dk::py_atan2(y.to_double(), x.to_double());
    double phi = dk::py_atan2(z.to_double(), p.to_double() * (1 - e2.to_double()));   // a start; the iteration does the work
    for (int iteration = 0; iteration < 60; ++iteration) {
        const Decimal sphi = from_double(dk::py_sin(phi));
        const Decimal n = wgs84_a() / (Decimal(1) - e2 * sphi * sphi).sqrt();
        const double next = dk::py_atan2((z + e2 * n * sphi).to_double(), p.to_double());
        if (std::fabs(next - phi) < 1e-17) {
            phi = next;
            break;
        }
        phi = next;
    }
    const Decimal sphi = from_double(dk::py_sin(phi));
    const Decimal cphi = from_double(dk::py_cos(phi));
    const Decimal n = wgs84_a() / (Decimal(1) - e2 * sphi * sphi).sqrt();
    const Decimal h = p * cphi + z * sphi - wgs84_a() * wgs84_a() / n;   // h = p cos(phi) + z sin(phi) - a^2 / N
    return Geodetic{phi, lon, h.to_double()};
}

std::pair<Decimal, Decimal> lt_closed_form(int event, const Vec& s0, const Vec& vs, const Vec& r0, const Vec& vr) {
    const Decimal& c = speed_of_light();
    Vec e;
    Decimal tau_u;
    if (event == 2) {
        const Vec big_d = sub(r0, s0);
        const Decimal vr2 = dot(vr, vr);
        const Decimal k = c * c - vr2;
        const Decimal dv = dot(big_d, vr);
        tau_u = (dv + (dv * dv + k * dot(big_d, big_d)).sqrt()) / k;
        const Vec rb = add_scaled(r0, vr, tau_u);
        const Vec sb = add_scaled(s0, vs, tau_u);
        e = sub(rb, sb);
    } else {
        e = sub(r0, s0);
        const Decimal vs2 = dot(vs, vs);
        const Decimal k = c * c - vs2;
        const Decimal ev = dot(e, vs);
        tau_u = (ev + (ev * ev + k * dot(e, e)).sqrt()) / k;
    }
    const Decimal vs2 = dot(vs, vs);
    const Decimal k = c * c - vs2;
    const Decimal ev = dot(e, vs);
    const Decimal tau_d = (-ev + (ev * ev + k * dot(e, e)).sqrt()) / k;
    return {tau_u, tau_d};
}

std::pair<Decimal, Decimal> lt_closed_form(int event, const std::array<double, 3>& s0, const std::array<double, 3>& vs, const std::array<double, 3>& r0, const std::array<double, 3>& vr) {
    return lt_closed_form(event, to_vec(s0), to_vec(vs), to_vec(r0), to_vec(vr));
}

Values registry_section(const std::filesystem::path& root) {
    const std::vector<std::string> slrf = read_lines(root / kSlrfRelative);
    const std::vector<std::string> ecc = read_lines(root / kEccRelative);
    std::map<std::string, Decimal> est;
    for (const std::string& line : block(slrf, "SOLUTION/ESTIMATE")) {
        const std::vector<std::string> f = dk::split_py(line);
        if (field(f, 2, "SOLUTION/ESTIMATE") == "7090" && field(f, 3, "SOLUTION/ESTIMATE") == "A" && field(f, 4, "SOLUTION/ESTIMATE") == "1") {
            est.insert_or_assign(f[1], number(field(f, 8, "SOLUTION/ESTIMATE"), "the estimate of Yarragadee"));   // (f[1] exists: f[2], f[3] and f[4] were read first)
        }
    }
    for (const char* name : {"STAX", "STAY", "STAZ", "VELX", "VELY", "VELZ"}) {
        if (est.count(name) == 0) throw MissingInput(std::string("SLRF2020 holds no ") + name + " estimate for 7090 A 1");
    }
    if (est.size() != 6) throw MissingInput("SLRF2020 holds estimates of other types for 7090 A 1");
    // the eccentricity of SOD 70900513 whose span is open-ended (14:080:00000 ... 00:000:00000)
    std::vector<std::vector<std::string>> rows;
    for (const std::string& line : block(ecc, "SITE/ECCENTRICITY")) {
        const std::vector<std::string> f = dk::split_py(line);
        if (field(f, f.empty() ? 0 : f.size() - 1, "SITE/ECCENTRICITY") == "70900513" && field(f, 5, "SITE/ECCENTRICITY") == "00:000:00000") rows.push_back(f);
    }
    if (rows.size() != 1) throw MissingInput("the eccentricity file holds " + std::to_string(rows.size()) + " open-ended rows of SOD 70900513, one is wanted");
    const Decimal u = number(field(rows[0], 7, "SITE/ECCENTRICITY"), "the eccentricity");
    const Decimal n = number(field(rows[0], 8, "SITE/ECCENTRICITY"), "the eccentricity");
    const Decimal e = number(field(rows[0], 9, "SITE/ECCENTRICITY"), "the eccentricity");
    // the reference epoch is 15:001:00000 = 2015-01-01 00:00:00 UTC; the epoch is 2026-01-01 02:07:56.8005871 UTC.  Elapsed SI time = elapsed UTC time + the leap seconds between: TAI-UTC was 35 s on 2015-01-01
    // (2012-07-01) and 37 s from 2017-01-01, so 2 s (the 2015-06-30 and 2016-12-31 leap seconds).
    const Decimal days(days_from_civil(2026, 1, 1) - days_from_civil(2015, 1, 1));
    const Decimal elapsed_si = days * Decimal(86400) + D("7676.8005871") + Decimal(2);
    const Decimal years = elapsed_si / (D("365.25") * Decimal(86400));
    const Decimal mx = est.at("STAX") + est.at("VELX") * years;
    const Decimal my = est.at("STAY") + est.at("VELY") * years;
    const Decimal mz = est.at("STAZ") + est.at("VELZ") * years;
    const Geodetic marker = geodetic(mx, my, mz);
    const Decimal sp = from_double(dk::py_sin(marker.latitude));
    const Decimal cp = from_double(dk::py_cos(marker.latitude));
    const Decimal sl = from_double(dk::py_sin(marker.longitude));
    const Decimal cl = from_double(dk::py_cos(marker.longitude));
    const Decimal sx = mx + cp * cl * u - sp * cl * n - sl * e;
    const Decimal sy = my + cp * sl * u - sp * sl * n + cl * e;
    const Decimal sz = mz + sp * u + cp * n;
    const Geodetic srp = geodetic(sx, sy, sz);
    const Decimal dx = sx - mx;
    const Decimal dy = sy - my;
    const Decimal dz = sz - mz;
    const Decimal two(2);
    return {
        {"yarl_elapsed_years", years.to_double()},
        {"yarl_marker_x_m", mx.to_double()}, {"yarl_marker_y_m", my.to_double()}, {"yarl_marker_z_m", mz.to_double()},
        {"yarl_marker_lat_rad", marker.latitude}, {"yarl_marker_lon_rad", marker.longitude}, {"yarl_marker_h_m", marker.height},
        {"yarl_srp_x_m", sx.to_double()}, {"yarl_srp_y_m", sy.to_double()}, {"yarl_srp_z_m", sz.to_double()},
        {"yarl_srp_lat_rad", srp.latitude}, {"yarl_srp_lon_rad", srp.longitude}, {"yarl_srp_h_m", srp.height},
        {"yarl_srp_minus_marker_m", (dx.pow(two) + dy.pow(two) + dz.pow(two)).sqrt().to_double()},
    };
}

Values shapiro_section() {
    const Decimal gm = D("3.986004415e14");   // TN36-1, TT-compatible
    const Decimal& c = speed_of_light();
    const Decimal k = Decimal(2) * gm / (c * c);
    const Decimal r1 = Decimal(6378137);   // a station on the ellipsoid, spherical stand-in
    // the geometry: the three DOUBLES the test passes (r1, r2, rho)
    const auto geometry = [&](const Decimal& r2, const Decimal& sin_e) {
        const Decimal rho = -r1 * sin_e + (r1 * r1 * sin_e * sin_e + r2 * r2 - r1 * r1).sqrt();
        return std::array<double, 3>{r1.to_double(), r2.to_double(), rho.to_double()};
    };
    Values out;
    out.emplace_back("shapiro_two_gm_over_c2_m", k.to_double());
    struct Case {
        const char* name;
        std::array<double, 3> values;
    };
    const std::array<Case, 3> cases = {{{"zenith", geometry(Decimal(12270000), Decimal(1))},
                                        {"el10", geometry(Decimal(12270000), from_double(dk::py_sin(dk::py_radians(10.0))))},
                                        {"leo30", geometry(D("7400000"), from_double(dk::py_sin(dk::py_radians(30.0))))}}};
    for (const Case& cs : cases) {
        const std::string name = cs.name;
        const double a = cs.values[0];
        const double b = cs.values[1];
        const double rho = cs.values[2];
        const Decimal x1 = from_double(a);   // the DOUBLES the test passes, exactly
        const Decimal x2 = from_double(b);
        const Decimal xr = from_double(rho);
        const auto shapiro = [&](const Decimal& p1, const Decimal& p2, const Decimal& pr) { return k * ((p1 + p2 + pr) / (p1 + p2 - pr)).ln(); };
        const Decimal delay = shapiro(x1, x2, xr);
        const Decimal s = x1 + x2;
        const Decimal d_rho = k * Decimal(2) * s / (s * s - xr * xr);
        const Decimal d_r = -k * Decimal(2) * xr / (s * s - xr * xr);   // d/dr1 = d/dr2
        // the two-method guard: a 60-digit central difference must agree with the closed form to 1e-30 relative
        const Decimal h = D("1e-20");
        const Decimal n_rho = (shapiro(x1, x2, xr + h) - shapiro(x1, x2, xr - h)) / (Decimal(2) * h);
        const Decimal n_r1 = (shapiro(x1 + h, x2, xr) - shapiro(x1 - h, x2, xr)) / (Decimal(2) * h);
        const Decimal n_r2 = (shapiro(x1, x2 + h, xr) - shapiro(x1, x2 - h, xr)) / (Decimal(2) * h);
        const std::array<std::pair<const Decimal*, const Decimal*>, 3> pairs = {{{&d_rho, &n_rho}, {&d_r, &n_r1}, {&d_r, &n_r2}}};
        for (const auto& [closed, numerical] : pairs) {
            if (!((*closed - *numerical).abs() <= closed->abs() * D("1e-30"))) {
                throw GuardFailed("shapiro: the closed form " + closed->to_string() + " and the central difference " + numerical->to_string() + " of the partials disagree at " + name);
            }
        }
        out.emplace_back("shapiro_" + name + "_r1_m", a);
        out.emplace_back("shapiro_" + name + "_r2_m", b);
        out.emplace_back("shapiro_" + name + "_rho_m", rho);
        out.emplace_back("shapiro_" + name + "_delay_m", delay.to_double());
        out.emplace_back("shapiro_" + name + "_d_rho", d_rho.to_double());
        out.emplace_back("shapiro_" + name + "_d_r", d_r.to_double());
    }
    return out;
}

Values vapour_section() {
    Values out;
    struct Row {
        const char* tag;
        double t;
        double rh;
    };
    for (const Row& row : {Row{"a", 20.0, 0.5}, Row{"b", 0.0, 1.0}, Row{"c", -10.0, 0.3}, Row{"d", 35.0, 0.8}}) {
        const Decimal dt = from_double(row.t);   // the doubles the test passes, exactly
        const Decimal drh = from_double(row.rh);
        const Decimal e = drh * D("6.11") * d_pow10(D("7.5") * dt / (D("237.3") + dt));
        const std::string tag = row.tag;
        out.emplace_back("vapour_" + tag + "_t_c", row.t);
        out.emplace_back("vapour_" + tag + "_rh", row.rh);
        out.emplace_back("vapour_" + tag + "_e_hpa", e.to_double());
    }
    return out;
}

Values zenith_section() {
    // the printed inputs of the IERS FCUL_ZD_HPA prolog's test case (a published observation, the routine named and cited)
    const Decimal lat_deg = D("30.67166667");
    const Decimal height = D("2010.344");
    const Decimal ps = D("798.4188");
    const Decimal es = D("14.322");
    const Decimal lam = D("0.532");
    const Decimal lat = lat_deg * pi() / Decimal(180);
    const Decimal lat_d = from_double(lat.to_double());   // the double the test passes
    const Decimal fs = Decimal(1) - D("0.00266") * d_cos(Decimal(2) * lat_d) - D("0.00000028") * height;
    const Decimal k0 = D("238.0185");
    const Decimal k2 = D("57.362");
    const Decimal k1s = D("19990.975");
    const Decimal k3s = D("579.55174");
    const Decimal sig2 = (Decimal(1) / lam).pow(Decimal(2));
    const Decimal cco2 = Decimal(1) + D("0.534e-6") * Decimal(375 - 450);
    const Decimal fh = D("1e-2") * (k1s * (k0 + sig2) / (k0 - sig2).pow(Decimal(2)) + k3s * (k2 + sig2) / (k2 - sig2).pow(Decimal(2))) * cco2;
    const Decimal w0 = D("295.235");
    const Decimal w1 = D("2.6422");
    const Decimal w2 = D("-0.032380");
    const Decimal w3 = D("0.004028");
    const Decimal fnh = D("0.003101") * (w0 + Decimal(3) * w1 * sig2 + Decimal(5) * w2 * sig2.pow(Decimal(2)) + Decimal(7) * w3 * sig2.pow(Decimal(3)));
    const Decimal zh = D("0.002416579") * fh / fs * ps;
    const Decimal zw = D("1e-4") * (D("5.316") * fnh - D("3.759") * fh) * es / fs;
    return {{"zenith_lat_rad", lat_d.to_double()},     {"zenith_height_m", height.to_double()},           {"zenith_p_hpa", ps.to_double()},          {"zenith_e_hpa", es.to_double()},
            {"zenith_lambda_um", lam.to_double()},     {"zenith_hydrostatic_m", zh.to_double()},          {"zenith_wet_m", zw.to_double()},          {"zenith_total_m", (zh + zw).to_double()}};
}

namespace {

// the first match of X =\s*([-+0-9.E]+)\s+Y =\s*([-+0-9.E]+)\s+Z =\s*([-+0-9.E]+) in the text: its three groups
std::optional<std::array<std::string, 3>> first_vector(const std::string& text) {
    const auto skip_space = [&](std::size_t i) {   // \s* from i
        while (i < text.size()) {
            std::size_t after = 0;
            if (!dk::is_py_space(dk::code_point_at(text, i, after))) break;
            i = after;
        }
        return i;
    };
    const auto token_end = [&](std::size_t i) {   // [-+0-9.E]+ from i (the end of the run: i itself when there is none)
        while (i < text.size() && (text[i] == '-' || text[i] == '+' || text[i] == '.' || text[i] == 'E' || (text[i] >= '0' && text[i] <= '9'))) ++i;
        return i;
    };
    for (std::size_t start = text.find("X ="); start != std::string::npos; start = text.find("X =", start + 1)) {
        std::array<std::string, 3> groups;
        std::size_t p = start + 3;
        bool matched = true;
        for (std::size_t g = 0; g < 3 && matched; ++g) {
            if (g > 0) {   // \s+ then the letter and " ="
                const std::size_t q = skip_space(p);
                const std::string label = g == 1 ? "Y =" : "Z =";
                if (q == p || text.compare(q, 3, label) != 0) {
                    matched = false;
                    break;
                }
                p = q + 3;
            }
            p = skip_space(p);
            const std::size_t end = token_end(p);
            if (end == p) {
                matched = false;
                break;
            }
            groups[g] = text.substr(p, end - p);
            p = end;
        }
        if (matched) return groups;
    }
    return std::nullopt;
}

}  // namespace

Values position_section(const std::filesystem::path& root) {
    const std::string hz = read_whole(root / kHorizonsRelative);
    const std::size_t soe_at = hz.find("$$SOE");
    if (soe_at == std::string::npos) throw MissingInput((root / kHorizonsRelative).string() + ": no $$SOE marker");
    const std::string soe = hz.substr(soe_at);
    const std::optional<std::array<std::string, 3>> groups = first_vector(soe);
    if (!groups) throw MissingInput((root / kHorizonsRelative).string() + ": no first Horizons record after $$SOE");
    const std::array<Decimal, 3> hz_km = {number((*groups)[0], "the first Horizons X"), number((*groups)[1], "the first Horizons Y"), number((*groups)[2], "the first Horizons Z")};
    std::optional<std::string> first_p;
    for (const std::string& line : read_lines(root / kSp3Relative)) {
        if (starts_with(line, "P")) {
            first_p = line;
            break;
        }
    }
    if (!first_p) throw MissingInput((root / kSp3Relative).string() + ": no P record");
    // SP3 P record: 'P', a three-character vehicle id, then three F14.6 fields in kilometres
    const auto slice = [&](std::size_t begin, std::size_t end) {   // line[begin:end] in code points
        std::string out;
        std::size_t i = 0;
        std::size_t index = 0;
        while (i < first_p->size() && index < end) {
            std::size_t after = 0;
            (void)dk::code_point_at(*first_p, i, after);
            if (index >= begin) out.append(*first_p, i, after - i);
            i = after;
            ++index;
        }
        return out;
    };
    std::array<Decimal, 3> sp3_km;
    for (std::size_t i = 0; i < 3; ++i) sp3_km[i] = number(slice(4 + 14 * i, 18 + 14 * i), "an SP3 coordinate");
    Values out;
    const std::array<std::pair<const char*, const std::array<Decimal, 3>*>, 2> sources = {{{"horizons", &hz_km}, {"sp3", &sp3_km}}};
    for (const auto& [name, km] : sources) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::string prefix = std::string("position_") + name + "_" + kAxes[axis];
            out.emplace_back(prefix + "_km", (*km)[axis].to_double());
            out.emplace_back(prefix + "_m", ((*km)[axis] * Decimal(1000)).to_double());
        }
        const Decimal two(2);
        out.emplace_back(std::string("position_") + name + "_norm_km", ((*km)[0].pow(two) + (*km)[1].pow(two) + (*km)[2].pow(two)).sqrt().to_double());
    }
    return out;
}

Values np_section() {
    const Decimal tof = D("0.051212898595");   // record 11 field 3 of the first normal point of lageos1_202601.np2 (a published observation)
    return {{"np_first_tof_s", tof.to_double()}, {"np_first_observed_range_m", (Decimal(299792458) * tof / Decimal(2)).to_double()}};
}

Values lighttime_section() {
    const Decimal& c = speed_of_light();
    Values out;
    const Decimal h = D("1e-20");
    for (const LtCase& g : lt_cases()) {
        const std::string name = g.name;
        const std::array<std::pair<const char*, const std::array<double, 3>*>, 4> inputs = {{{"s0", &g.s0}, {"vs", &g.vs}, {"r0", &g.r0}, {"vr", &g.vr}}};
        for (const auto& [key, v] : inputs) {
            for (std::size_t axis = 0; axis < 3; ++axis) out.emplace_back("lt_" + name + "_" + key + "_" + kAxes[axis], (*v)[axis]);
        }
        for (const int event : {2, 1}) {
            const auto [tau_u, tau_d] = lt_closed_form(event, g.s0, g.vs, g.r0, g.vr);
            const Decimal tof = tau_u + tau_d;
            const std::string prefix = "lt_" + name + "_e" + std::to_string(event);
            out.emplace_back(prefix + "_tau_u_s", tau_u.to_double());
            out.emplace_back(prefix + "_tau_d_s", tau_d.to_double());
            out.emplace_back(prefix + "_tof_s", tof.to_double());
            out.emplace_back(prefix + "_range_m", (c * tof / Decimal(2)).to_double());
            // the derivative of the range with respect to the target's position (a constant offset of the whole trajectory), by a 60-digit central difference of the closed form
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const auto shifted = [&](int sign) {
                    Vec r0 = to_vec(g.r0);
                    r0[axis] = r0[axis] + Decimal(sign) * h;
                    return lt_closed_form(event, to_vec(g.s0), to_vec(g.vs), r0, to_vec(g.vr));
                };
                const auto up = shifted(+1);
                const auto dn = shifted(-1);
                const Decimal d_range = c * ((up.first + up.second) - (dn.first + dn.second)) / (Decimal(2) * h) / Decimal(2);
                out.emplace_back(prefix + "_d_range_d" + kAxes[axis], d_range.to_double());
            }
        }
    }
    return out;
}

Values aberration_section() {
    // A(n; beta) = [n/gamma + (1 + (n.beta)/(1 + 1/gamma)) beta] / (1 + n.beta) and D(n; beta) = [n/gamma - (1 - (n.beta)/(1 + 1/gamma)) beta] / (1 - n.beta), each written out from the specification's own formulas
    // (MEAS-R-053), in 60 digits, for the DOUBLES the test passes.  D is written as its own formula, not as A(n; -beta).
    Values out;
    for (const AbCase& g : ab_cases()) {
        const std::string name = g.name;
        const Vec n = to_vec(g.n);
        const Vec b = to_vec(g.beta);
        const Decimal bb = dot(b, b);
        const Decimal inv_gamma = (Decimal(1) - bb).sqrt();
        const Decimal nb = dot(n, b);
        Vec a;
        Vec d;
        for (std::size_t i = 0; i < 3; ++i) {
            a[i] = (n[i] * inv_gamma + b[i] * (Decimal(1) + nb / (Decimal(1) + inv_gamma))) / (Decimal(1) + nb);
            d[i] = (n[i] * inv_gamma - b[i] * (Decimal(1) - nb / (Decimal(1) + inv_gamma))) / (Decimal(1) - nb);
        }
        // the two are inverse to each other in exact arithmetic: A(D(n)) = n, checked here in 60 digits before either is emitted
        const Decimal nbd = dot(d, b);
        for (std::size_t i = 0; i < 3; ++i) {
            const Decimal back = (d[i] * inv_gamma + b[i] * (Decimal(1) + nbd / (Decimal(1) + inv_gamma))) / (Decimal(1) + nbd);
            if (!((back - n[i]).abs() < D("1e-50"))) {
                throw GuardFailed("aberration: A(D(n)) is " + back.to_string() + ", not " + n[i].to_string() + " in the case " + name);
            }
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::string x(1, kAxes[axis]);
            out.emplace_back("ab_" + name + "_n_" + x, g.n[axis]);
            out.emplace_back("ab_" + name + "_beta_" + x, g.beta[axis]);
            out.emplace_back("ab_" + name + "_a_" + x, a[axis].to_double());
            out.emplace_back("ab_" + name + "_d_" + x, d[axis].to_double());
        }
    }
    return out;
}

Values emission_section() {
    // MEAS-A-088: the emission epoch and the geometric direction of a target in uniform motion seen from a fixed observer at the observation epoch, from the CLOSED FORM (c^2 - v^2) tau^2 + 2 (D.v) tau - D^2 = 0,
    // D = r(t_o) - s, v the target's velocity (the target at t_e = t_o - tau is at r - v tau and |r - v tau - s| = c tau); the direction is (D - v tau)/(c tau).  The two geometries of the light-time tests, with the
    // target position read as its position AT THE OBSERVATION epoch.
    const Decimal& c = speed_of_light();
    Values out;
    for (const LtCase& g : lt_cases()) {
        const std::string name = g.name;
        const Vec s = to_vec(g.s0);
        const Vec r = to_vec(g.r0);
        const Vec v = to_vec(g.vr);
        const Vec big_d = sub(r, s);
        const Decimal k = c * c - dot(v, v);
        const Decimal dv = dot(big_d, v);
        const Decimal tau = (-dv + (dv * dv + k * dot(big_d, big_d)).sqrt()) / k;
        Vec d;
        for (std::size_t i = 0; i < 3; ++i) d[i] = big_d[i] - v[i] * tau;
        if (!((dot(d, d).sqrt() - c * tau).abs() < D("1e-40"))) {   // the light-time condition holds
            throw GuardFailed("emission: the light-time condition fails in the case " + name);
        }
        out.emplace_back("em_" + name + "_tau_s", tau.to_double());
        out.emplace_back("em_" + name + "_rho_m", (c * tau).to_double());
        for (std::size_t axis = 0; axis < 3; ++axis) out.emplace_back("em_" + name + "_n_" + kAxes[axis], (d[axis] / (c * tau)).to_double());
    }
    return out;
}

Sections all_sections(const std::filesystem::path& root) {
    Sections sections;
    sections.emplace_back("registry", registry_section(root));
    sections.emplace_back("shapiro", shapiro_section());
    sections.emplace_back("vapour", vapour_section());
    sections.emplace_back("zenith", zenith_section());
    sections.emplace_back("np", np_section());
    sections.emplace_back("lighttime", lighttime_section());
    sections.emplace_back("position", position_section(root));
    sections.emplace_back("aberration", aberration_section());
    sections.emplace_back("emission", emission_section());
    return sections;
}

std::string render(const Sections& sections) {
    std::string out;
    const auto line = [&](const std::string& s) {
        out += s;
        out += '\n';
    };
    line("// GENERATED by tools/measmod_reference.cpp — do not edit; `measmod_reference --check` verifies it.");
    line("// Reference values computed independently of the code under test (see the generator's header).");
    line("#pragma once");
    line("");
    line("namespace odl::measmod::ref {");
    line("");
    for (const auto& [name, values] : sections) {
        line("// section: " + name);
        for (const auto& [key, value] : values) line("inline constexpr double " + key + " = " + dk::py_float_repr(value) + ";");
        line("");
    }
    line("}  // namespace odl::measmod::ref");
    return out;
}

namespace {

const char kUsageText[] = "usage: measmod_reference [-h] [--check] [--header HEADER] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "Reference values for SPEC-measmod.md's acceptance tests, from the DEFINITION, in 60-digit decimal arithmetic: the header modules/measmod/tests/measmod_reference.hpp.  --check regenerates it and fails\n"
    "if the committed one differs, so that a reference value cannot drift away from its generator.\n"
    "\n"
    "options:\n"
    "  -h, --help       show this help and exit\n"
    "  --check          fail (exit 1) unless the committed header is what the generator writes\n"
    "  --header HEADER  the header to write or to check (default: ROOT/modules/measmod/tests/measmod_reference.hpp)\n"
    "  --root ROOT      the tree whose data/ holds the inputs (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 written / the committed header matches   1 --check found a difference, or a guard of the generator refused\n"
    "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        bool check = false;
        std::filesystem::path root = settings.root;
        std::optional<std::filesystem::path> header_given;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--check" && !has_value) {
                check = true;
            } else if (opt == "--header" || opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--header") header_given = value;
                else root = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }
        const std::filesystem::path header = header_given ? *header_given : root / kHeaderRelative;

        // the Python's own state: its default decimal context and the precision it set at import
        LocalContext fresh;
        fresh.context() = DecimalContext{};
        fresh.context().prec = kPrecision;

        std::string text;
        try {
            text = render(all_sections(root));
        } catch (const MissingInput& exc) {
            io.err << kTool << ": " << exc.what() << " (run `tools/bootstrap.sh`)\n";
            return kArgument;
        } catch (const GuardFailed& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kFailed;
        }
        if (check) {
            std::error_code ec;
            bool same = false;
            if (std::filesystem::exists(header, ec)) {
                try {
                    same = dk::universal_newlines(dk::read_text(header)) == text;
                } catch (const std::runtime_error& exc) {
                    io.err << kTool << ": " << exc.what() << '\n';
                    return kArgument;
                }
            }
            if (!same) {
                io.err << "FAILED   " << header.string() << " differs from what the generator writes\n";
                return kFailed;
            }
            io.out << "ok       " << header.string() << " matches the generator\n";
            return kOk;
        }
        try {
            std::filesystem::create_directories(header.parent_path());
            dk::write_text(header, text);
        } catch (const std::exception& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
        io.out << "wrote    " << header.string() << '\n';
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::measmod_reference

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::measmod_reference::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
