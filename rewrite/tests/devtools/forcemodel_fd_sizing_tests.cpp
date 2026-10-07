// tests/devtools/forcemodel_fd_sizing_tests.cpp — the finite-difference gate's sizing (SPEC-forcemodel.md §6.2; plan L0 step 8, group C7): Lemma 1's constants and Lemma 2's Phi by hand, the bounds F and the
// registered noise bounds nu against the formulas of the docstring written out again (in long double), the power of the gate against its three wrong rows (a geometry worked out independently), the reading of the
// coefficient file, the power series of the looseness scan against series known in closed form and the whole scan on the one grid whose answer is known, the tool's command line and what it prints on a tree of
// its own, and the real tree against the output that the Python tool recorded (the L7 report's forcemodel_fd_sizing_check.out and forcemodel_fd_sizing_scan.out, embedded below).  (ctests
// `forcemodel_fd_sizing.behaviour` and `forcemodel_fd_sizing.real_tree`.)
//
// Every expectation is DERIVED BY HAND, from the lemmas and the f-strings of forcemodel_fd_sizing.py, and not taken from running the port; the one exception is the recorded output, which is the Python's.

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"
#include "throwing_stream.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/tool.hpp>

#include "forcemodel_fd_sizing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
namespace fd = odl::tools::forcemodel_fd_sizing;
namespace gr = odl::tools::gradient_reference;
using namespace odl::devkit;
using L = long double;

namespace {

constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }
bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_on(const fd::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = fd::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_real(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = fd::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool close(double got, L want, L relative) { return std::fabs(static_cast<L>(got) - want) <= relative * std::fabs(want); }

L lpow(L x, int n) {
    L out = 1;
    for (int i = 0; i < n; ++i) out *= x;
    return out;
}

constexpr const char* kEgmRelative = "data/cache/egm2008-coefficients/extracted/EGM2008_to2190_TideFree";

// the first four lines of the coefficient file, as the file has them (EGM2008 tide-free, the NGA's public numbers), and a fifth of degree 5 and a line that is not one: the reader must stop at the degree
const char kEgmHead[] =
    "    2    0   -0.484165143790815D-03    0.000000000000000D+00    0.7481239490D-11    0.0000000000D+00\n"
    "    2    1   -0.206615509074176D-09    0.138441389137979D-08    0.7063781502D-11    0.7348347201D-11\n"
    "    2    2    0.243938357328313D-05   -0.140027370385934D-05    0.7230231722D-11    0.7425816951D-11\n"
    "    3    0    0.957161207093473D-06    0.000000000000000D+00    0.5731430751D-11    0.0000000000D+00\n";

/// a tree with a coefficient file of the given text
struct Tree {
    TempDir td{"odl-fd"};
    fs::path root = td.path();
    explicit Tree(const std::string& egm_text) {
        fs::create_directories((root / kEgmRelative).parent_path());
        write_text(root / kEgmRelative, egm_text);
    }
    fd::Settings settings() const {
        fd::Settings s = fd::default_settings();
        s.root = root;
        return s;
    }
};

// the output of `forcemodel_fd_sizing.py --scan` as the L7 report recorded it (forcemodel_fd_sizing_scan.out, 2,502 bytes); its first 2,088 bytes are the output of --check (forcemodel_fd_sizing_check.out)
const char kRecordedScan[] = R"GOLDEN(ok       S3(1,3): 96
ok       S3(1,4): 180
ok       S3(2,5): 420
ok       S3(0,3): 60
ok       S3(3,7): 1140
ok       S3(1,5): 300
ok       Phi_20: 26832.815729997477
ok       Phi_point_mass: 96.0

Gravity(N = 4): rigorous F at r_min = r - 1000 m, the registered nu = 64 eps |a|, eps(h), and the best relative defect it can see
  point                   r (m)    F (m^-2 s^-2)   nu (m s^-2)    eps(10m)    eps(30m)   eps(100m)   eps(300m)  eps(1000m)   min eps / |G|
  GPS                26561000.0       2.9480e-21     8.029e-15   8.030e-16   2.681e-16   8.520e-17   7.098e-17   4.994e-16   1.67e-09
  LEO 300 km          6678136.3       3.4168e-18     1.270e-13   1.276e-14   4.746e-15   6.965e-15   5.168e-14   5.696e-13   1.77e-09
  LEO 952.86 km       7330996.3       2.0779e-18     1.054e-13   1.057e-14   3.825e-15   4.517e-15   3.152e-14   3.464e-13   1.89e-09
  sail 720 km         7098136.3       2.4663e-18     1.124e-13   1.128e-14   4.117e-15   5.235e-15   3.737e-14   4.112e-13   1.85e-09

Gravity(N = 4): the three wrong rows' predicted power, max |wrong - right| / (eps at the best step); the registered requirement is >= 1e3
  GPS              ERA  149.377 deg  |sin ERA| = 0.509  unrotated: 4.28e+08  transposed: 6.16e+08  sign flipped: 8.86e+08
  LEO 300 km       ERA   28.965 deg  |sin ERA| = 0.484  unrotated: 3.58e+08  transposed: 6.07e+08  sign flipped: 1.13e+09
  LEO 952.86 km    ERA   28.965 deg  |sin ERA| = 0.484  unrotated: 3.36e+08  transposed: 5.70e+08  sign flipped: 1.06e+09
  sail 720 km      ERA  278.513 deg  |sin ERA| = 0.989  unrotated: 6.24e+08  transposed: 2.05e+08  sign flipped: 8.70e+08

Third body (mu / d_min^5 x 96), the Sun and the Moon at a LEO point:
  Sun   F = 1.701e-34  nu = 4.214e-17  |G| ~ 7.928e-14  best eps = 4.214e-20  relative 5.31e-07
  Moon  F = 6.148e-29  nu = 2.358e-19  |G| ~ 1.726e-13  best eps = 2.460e-22  relative 1.43e-09

Relativity at the LEO 300 km point (v = sqrt(GM/r)):
  Schwarzschild  F = 2.954e-26  nu = 1.265e-22   Lense-Thirring  F = 1.699e-27  nu = 1.602e-24   de Sitter  F = 0 (d a / d r = 0 identically)

Looseness, the TRUE supremum of |d_e^3 a_i| against F (point mass + degrees 2..4 of EGM2008; coarse grid, exact series):
  GPS              true sup 6.509e-22   F 2.948e-21   F / true sup = 4.5
  LEO 300 km       true sup 6.421e-19   F 3.417e-18   F / true sup = 5.3
  LEO 952.86 km    true sup 4.034e-19   F 2.078e-18   F / true sup = 5.2
  sail 720 km      true sup 4.738e-19   F 2.466e-18   F / true sup = 5.2
)GOLDEN";
constexpr std::size_t kRecordedCheckBytes = 2088;

}  // namespace

// ======================================================================================================================================== Lemma 1 and Lemma 2

TEST_CASE("S3(d, q) = sum_j C(3,j) d^(falling 3-j) (q)_j, worked out by hand", "[forcemodel_fd_sizing][behaviour]") {
    // S3(1,3) = 3*1*12 + 60 = 96;  S3(1,4) = 3*1*20 + 120 = 180;  S3(2,5) = 6*5 + 6*30 + 210 = 420;  S3(0,3) = (3)_3 = 60;  S3(3,7) = 6 + 126 + 504 + 504 = 1140;  S3(1,5) = 90 + 210 = 300      (the Python's comments)
    // S3(2,2) = 0 + 3*2*2 + 3*2*6 + 24 = 72;  S3(3,1) = 6 + 3*6*1 + 3*3*2 + 6 = 48;  S3(0,0) = 0 (every term has a factor (0)_j or 0 falling);  S3(4,0) = 24 (only j = 0 has no factor 0);
    // S3(2,7) = 0 + 3*2*7 + 3*2*56 + 504 = 882;  S3(4,9) = 24 + 3*12*9 + 3*4*90 + 990 = 2418
    const std::vector<std::tuple<int, int, long long>> table = {{1, 3, 96}, {1, 4, 180}, {2, 5, 420}, {0, 3, 60}, {3, 7, 1140}, {1, 5, 300}, {2, 2, 72}, {3, 1, 48}, {0, 0, 0}, {4, 0, 24}, {2, 7, 882}, {4, 9, 2418}};
    for (const auto& [d, q, want] : table) {
        INFO("S3(" << d << "," << q << ")");
        CHECK(fd::S3(d, q) == BigInt(want));
    }
    // d = 1, q = 3 again, by the closed form of the Leibniz sum for d = 1: S3(1, q) = 3 q (q+1) + q (q+1) (q+2)
    for (int q = 1; q < 12; ++q) CHECK(fd::S3(1, q) == BigInt(3 * q * (q + 1) + q * (q + 1) * (q + 2)));
}

TEST_CASE("l1 is the sum of the absolute values of a polynomial's coefficients", "[forcemodel_fd_sizing][behaviour]") {
    gr::Poly p;
    p.set({1, 0, 0}, Rational(BigInt(-3), BigInt(2)));
    p.set({0, 1, 0}, Rational(BigInt(1), BigInt(3)));
    p.set({0, 0, 2}, Rational(BigInt(5)));
    CHECK(fd::l1(p) == Rational(BigInt(41), BigInt(6)));   // 3/2 + 1/3 + 5
    CHECK(fd::l1(gr::Poly{}) == Rational(0));
}

TEST_CASE("Phi_nm, by hand from the polynomials: (2,0,C), (2,1,C), (2,2,C), (3,0,C), the zero polynomial, and the point mass", "[forcemodel_fd_sizing][behaviour]") {
    // Phi_nm = N_nm ( max_i ||d_i q|| S3(n-1, 2n+1) + (2n+1) ||q|| S3(n+1, 2n+3) )
    //  (2,0,C): q = z^2 - (x^2 + y^2)/2, ||q|| = 2, max ||d_i q|| = 2 (d_z q = 2z):  sqrt(5) (2 * 300 + 10 * 1140) = sqrt(5) * 12000
    //  (2,1,C): q = 3 x z, ||q|| = 3, ||d_x q|| = ||d_z q|| = 3:                      sqrt(5/3) (3 * 300 + 5 * 3 * 1140) = sqrt(5/3) * 18000
    //  (2,2,C): q = 3 (x^2 - y^2), ||q|| = 6, ||d_x q|| = ||d_y q|| = 6:              sqrt(5/12) (6 * 300 + 5 * 6 * 1140) = sqrt(5/12) * 36000
    //  (3,0,C): q = z^3 - 3 z (x^2 + y^2)/2, ||q|| = 4, ||d_z q|| = 3 + 3/2 + 3/2 = 6: sqrt(7) (6 * S3(2,7) + 7 * 4 * S3(4,9)) = sqrt(7) (6 * 882 + 28 * 2418) = sqrt(7) * 72996
    CHECK(close(fd::phi(2, 0, 'C'), std::sqrt(static_cast<L>(5)) * 12000, 1e-13L));
    CHECK(close(fd::phi(2, 1, 'C'), std::sqrt(static_cast<L>(5) / 3) * 18000, 1e-13L));
    CHECK(close(fd::phi(2, 1, 'S'), std::sqrt(static_cast<L>(5) / 3) * 18000, 1e-13L));   // 3 y z: the same norms
    CHECK(close(fd::phi(2, 2, 'C'), std::sqrt(static_cast<L>(5) / 12) * 36000, 1e-13L));
    CHECK(close(fd::phi(3, 0, 'C'), std::sqrt(static_cast<L>(7)) * 72996, 1e-13L));
    CHECK(fd::phi(2, 0, 'S') == 0.0);   // the zero polynomial: no potential
    CHECK(fd::phi(3, 0, 'S') == 0.0);
    CHECK(fd::phi_point_mass() == 96.0);
}

// ======================================================================================================================================== the bounds F and nu, and eps(h)

TEST_CASE("the bounds of the third body and of the relativistic accelerations, and the registered noise bounds, written out again from the docstring", "[forcemodel_fd_sizing][behaviour]") {
    // F = mu 96 / d_min^5
    CHECK(fd::f_third_body(1.0, 2.0) == 3.0);                // 96 / 32
    CHECK(fd::f_third_body(1e10, 10.0) == 9.6e6);            // 9.6e11 / 1e5, both exact
    // nu: 64 eps |a|, 16 eps mu (1/d^2 + 1/s^2), 32 eps |a|, 128 eps |a|, with eps = 2^-52 (every one of these is a power of two times a small number: exact)
    CHECK(fd::nu_gravity(1.0) == 0x1p-46);
    CHECK(fd::nu_relativity(8.0) == 0x1p-44);                // 32 * 2^-52 * 8
    CHECK(fd::nu_tides(2.0) == 0x1p-44);                     // 128 * 2^-52 * 2
    CHECK(fd::nu_third_body(1.0, 2.0, 4.0) == 5.0 * 0x1p-52);   // 16 * 2^-52 * 1 * (1/4 + 1/16) = 5 * 2^-52
    CHECK(fd::kEps == 0x1p-52);
    // eps(h) = h^2 F / 6 + nu / h
    CHECK(fd::eps_h(6.0, 1.0, 2.0) == 4.5);                  // 4 * 6 / 6 + 1/2
    CHECK(fd::eps_h(12.0, 3.0, 3.0) == 19.0);                // 9 * 12 / 6 + 1
    // Schwarzschild: k = GM/c^2;  F = 4 GM k S3(1,4) r^-6 + k v^2 S3(1,3) r^-5 + 4 k v (sqrt(3) v) S3(1,3) r^-5
    {
        const L gm = 3.986004418e14L, c = 299792458.0L, r = 6677136.3L, v = 7725.8L;
        const L k = gm / (c * c);
        const L want = 4 * gm * k * 180 / lpow(r, 6) + k * v * v * 96 / lpow(r, 5) + 4 * k * v * (std::sqrt(static_cast<L>(3)) * v) * 96 / lpow(r, 5);
        CHECK(close(fd::f_schwarzschild(6677136.3, 7725.8), want, 1e-12L));
        // Lense-Thirring: k = 2 GM/c^2, J = 9.8e8;  F = k ( 3 J (sqrt(3) v) S3(2,5) r^-6 + J v S3(0,3) r^-6 )
        const L kl = 2 * gm / (c * c);
        const L j = 9.8e8L;
        const L wantl = kl * (3 * j * (std::sqrt(static_cast<L>(3)) * v) * 420 / lpow(r, 6) + j * v * 60 / lpow(r, 6));
        CHECK(close(fd::f_lense_thirring(6677136.3, 7725.8), wantl, 1e-12L));
    }
}

TEST_CASE("the four reference points are L4's: the radii, the Earth-rotation angles and the phases", "[forcemodel_fd_sizing][behaviour]") {
    const auto& points = fd::reference_points();
    CHECK(std::string(points[0].name) == "GPS");
    CHECK(points[0].r == 26561e3);
    CHECK(points[0].era_deg == 149.377);
    CHECK(points[0].phase == 0.7);
    CHECK(std::string(points[1].name) == "LEO 300 km");
    CHECK(points[1].r == 6378136.3 + 300e3);
    CHECK(points[1].era_deg == 28.965);
    CHECK(points[1].phase == 0.0);
    CHECK(std::string(points[2].name) == "LEO 952.86 km");
    CHECK(points[2].r == 6378136.3 + 952.86e3);
    CHECK(points[2].era_deg == 28.965);
    CHECK(std::string(points[3].name) == "sail 720 km");
    CHECK(points[3].r == 6378136.3 + 720e3);
    CHECK(points[3].era_deg == 278.513);
    CHECK(points[3].phase == 1.2);
    CHECK(fd::kHs == (std::array<double, 5>{10.0, 30.0, 100.0, 300.0, 1000.0}));
    CHECK(fd::kHMax == 1000.0);
}

// ======================================================================================================================================== the coefficient file

TEST_CASE("coefficients: the lines up to the degree asked for, in the file's order, the D of Fortran read as E; a file that is not there is nothing, one that is not readable is an error", "[forcemodel_fd_sizing][behaviour]") {
    TempDir dir("odl-fd");
    const fs::path file = dir.path() / "egm";
    write_text(file, std::string(kEgmHead) + "    5    0    0.100000000000000D-05    0.000000000000000D+00    0.1D-11    0.0D+00\n" + "this is not a coefficient line\n");
    const auto up_to_4 = fd::coefficients(file, 4);
    REQUIRE(up_to_4.has_value());
    REQUIRE(up_to_4->size() == 4);   // the degree-5 line stops the reading, and what follows it is never looked at
    CHECK((*up_to_4)[0].n == 2);
    CHECK((*up_to_4)[0].m == 0);
    CHECK((*up_to_4)[0].c == -0.484165143790815e-03);
    CHECK((*up_to_4)[0].s == 0.0);
    CHECK((*up_to_4)[1].m == 1);
    CHECK((*up_to_4)[1].c == -0.206615509074176e-09);
    CHECK((*up_to_4)[1].s == 0.138441389137979e-08);
    CHECK((*up_to_4)[2].m == 2);
    CHECK((*up_to_4)[2].c == 0.243938357328313e-05);
    CHECK((*up_to_4)[2].s == -0.140027370385934e-05);
    CHECK((*up_to_4)[3].n == 3);
    CHECK((*up_to_4)[3].m == 0);
    CHECK((*up_to_4)[3].c == 0.957161207093473e-06);
    const auto up_to_2 = fd::coefficients(file, 2);
    REQUIRE(up_to_2.has_value());
    CHECK(up_to_2->size() == 3);
    const auto none = fd::coefficients(file, 1);
    REQUIRE(none.has_value());
    CHECK(none->empty());
    // a file that is not there
    CHECK_FALSE(fd::coefficients(dir.path() / "absent", 4).has_value());
    // not a regular file, and lines that are not coefficient lines
    CHECK_THROWS_AS(fd::coefficients(dir.path(), 4), std::runtime_error);
    for (const char* bad : {"2\n", "2 0 abc 0.0\n", "x 0 1.0D-3 0.0\n", "2x 0 1.0D-3 0.0\n", "2 0x 1.0D-3 0.0\n", "2 0 1.0D-3\n", "2 0 1.0D-3 1.0D-3x\n", "\n"}) {
        write_text(file, bad);
        INFO(bad);
        CHECK_THROWS_AS(fd::coefficients(file, 4), std::runtime_error);
    }
    write_text(file, "2 0 1.0D-3\n");
    try {
        (void)fd::coefficients(file, 4);
        FAIL("a line with three fields was taken for a coefficient line");
    } catch (const std::runtime_error& exc) {
        CHECK(std::string(exc.what()) == file.string() + ": line 1 is not a coefficient line");
    }
    // a line beyond the degree asked for is not looked at beyond its degree
    write_text(file, "2 0 1.0D-3 2.0D-3\n9 9\n");
    const auto stopped = fd::coefficients(file, 2);
    REQUIRE(stopped.has_value());
    CHECK(stopped->size() == 1);
    // a file that is there and is a file but cannot be opened says so (a process that may read any file, root, reads this one too, and the check is left out for it)
    write_text(file, "2 0 1.0D-3 2.0D-3\n");
    fs::permissions(file, fs::perms::none);
    if (::geteuid() != 0) {
        try {
            (void)fd::coefficients(file, 4);
            FAIL("an unreadable file was read");
        } catch (const std::runtime_error& exc) {
            CHECK(std::string(exc.what()) == "cannot read " + file.string());
        }
    }
    fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
}

// ======================================================================================================================================== F of the gravity field, the table, the power of the gate

TEST_CASE("F of the gravity field is 1.01 times the point mass and each coefficient's Phi: with no coefficient, the point mass alone", "[forcemodel_fd_sizing][behaviour]") {
    const L gm = 3.986004415e14L;
    const L r = 6677136.3L;
    CHECK(close(fd::f_gravity(4, 6677136.3, {}), 1.01L * gm * 96 / lpow(r, 5), 1e-13L));
    // one coefficient (2, 1) with C = c and S = s: + GM a_e^2 (|c| Phi(2,1,C) + |s| Phi(2,1,S)) / r^7;  and with m = 0 only C counts
    const std::vector<fd::Coefficient> one = {{2, 1, -2.0e-10, 1.5e-9}};
    const L phi21 = std::sqrt(static_cast<L>(5) / 3) * 18000;
    const L ae = 6378136.3L;
    CHECK(close(fd::f_gravity(4, 6677136.3, one), 1.01L * (gm * 96 / lpow(r, 5) + gm * ae * ae * (2.0e-10L * phi21 + 1.5e-9L * phi21) / lpow(r, 7)), 1e-12L));
    const std::vector<fd::Coefficient> zonal = {{2, 0, -4.84e-4, 7.0e-3}};   // the S of an m = 0 line is not a coefficient, whatever the file says
    const L phi20 = std::sqrt(static_cast<L>(5)) * 12000;
    CHECK(close(fd::f_gravity(4, 6677136.3, zonal), 1.01L * (gm * 96 / lpow(r, 5) + gm * ae * ae * (4.84e-4L * phi20) / lpow(r, 7)), 1e-12L));
}

TEST_CASE("the table of the sizing, row by row, from F and nu and eps(h) with no coefficient", "[forcemodel_fd_sizing][behaviour]") {
    const std::vector<fd::TableRow> rows = fd::table({});
    REQUIRE(rows.size() == 4);
    const L gm = 3.986004415e14L;
    for (std::size_t k = 0; k < 4; ++k) {
        const auto& point = fd::reference_points()[k];
        INFO(point.name);
        CHECK(rows[k].name == point.name);
        CHECK(rows[k].r == point.r);
        const L r = point.r;
        const L f = 1.01L * gm * 96 / lpow(r - 1000, 5);
        const L nu = 64 * 0x1p-52L * gm / (r * r);
        CHECK(close(rows[k].f, f, 1e-13L));
        CHECK(close(rows[k].nu, nu, 1e-13L));
        L best = 1e300L;
        for (std::size_t h = 0; h < 5; ++h) {
            const L hh = fd::kHs[h];
            const L e = hh * hh * f / 6 + nu / hh;
            CHECK(close(rows[k].e[h], e, 1e-12L));
            best = std::min(best, e);
        }
        CHECK(close(rows[k].rel, best / (2 * gm / (r * r * r)), 1e-12L));
    }
}

TEST_CASE("the power of the gate: the three wrong rows against the best eps, for a geometry worked out independently", "[forcemodel_fd_sizing][behaviour]") {
    // The point-mass tensor of a unit vector u at the angle a in the equatorial plane is mu/r^3 (3 u u^T - I): xx = 3 cos^2 a - 1, xy = 3 sin a cos a, yy = 3 sin^2 a - 1, zz = -1.  The right one is at the
    // phase; the unrotated row is the ITRS direction used as if it were GCRS: the angle phase - ERA; the transposed row, the angle phase - 2 ERA; the sign flipped row is -G.  Each row is
    // max |wrong - right| / (eps at the best step), and with no coefficient F is the point mass's.
    const std::vector<fd::ControlPower> power = fd::control_power({});
    REQUIRE(power.size() == 4);
    const L gm = 3.986004415e14L;
    for (std::size_t k = 0; k < 4; ++k) {
        const auto& point = fd::reference_points()[k];
        INFO(point.name);
        CHECK(power[k].name == point.name);
        CHECK(power[k].th == point.era_deg * (std::numbers::pi / 180.0));   // math.radians
        const L r = point.r;
        const L mu_r3 = gm / (r * r * r);
        const L f = 1.01L * gm * 96 / lpow(r - 1000, 5);
        const L nu = 64 * 0x1p-52L * gm / (r * r);
        L best = 1e300L;
        for (const double h : fd::kHs) best = std::min(best, static_cast<L>(h) * h * f / 6 + nu / h);
        const L th = static_cast<L>(point.era_deg) * std::numbers::pi_v<L> / 180;
        const L phase = point.phase;
        const auto tensor_diff = [&](L angle) {
            const L c0 = std::cos(phase), s0 = std::sin(phase), c1 = std::cos(angle), s1 = std::sin(angle);
            L worst = std::fabs(3 * (c1 * c1 - c0 * c0));
            worst = std::max(worst, std::fabs(3 * (s1 * c1 - s0 * c0)));
            worst = std::max(worst, std::fabs(3 * (s1 * s1 - s0 * s0)));
            return worst * mu_r3 / best;
        };
        L biggest = 1;   // |zz| = 1
        {
            const L c0 = std::cos(phase), s0 = std::sin(phase);
            biggest = std::max({biggest, std::fabs(3 * c0 * c0 - 1), std::fabs(3 * s0 * c0), std::fabs(3 * s0 * s0 - 1)});
        }
        CHECK(power[k].row[0].first == "unrotated");
        CHECK(power[k].row[1].first == "transposed");
        CHECK(power[k].row[2].first == "sign flipped");
        CHECK(close(power[k].row[0].second, tensor_diff(phase - th), 1e-9L));
        CHECK(close(power[k].row[1].second, tensor_diff(phase - 2 * th), 1e-9L));
        CHECK(close(power[k].row[2].second, 2 * biggest * mu_r3 / best, 1e-9L));
    }
}

// ======================================================================================================================================== the looseness scan

TEST_CASE("the truncated power series: sums, products to the fourth term, and (g(t))^alpha by its recurrence against binomial series", "[forcemodel_fd_sizing][behaviour]") {
    using fd::Ser;
    CHECK(Ser{}.c == (std::array<double, 4>{0, 0, 0, 0}));
    CHECK(Ser{1.0, 2.0, 3.0, 4.0, 5.0}.c == (std::array<double, 4>{1, 2, 3, 4}));   // truncated after t^3
    CHECK(Ser{7.0}.c == (std::array<double, 4>{7, 0, 0, 0}));
    CHECK((Ser{1.0, 2.0} + Ser{3.0, 4.0, 5.0}).c == (std::array<double, 4>{4, 6, 5, 0}));
    // (1 + 2t + 3t^2 + 4t^3)(1 + t) = 1 + 3t + 5t^2 + 7t^3 + 4t^4: the t^4 is dropped
    CHECK((Ser{1.0, 2.0, 3.0, 4.0} * Ser{1.0, 1.0}).c == (std::array<double, 4>{1, 3, 5, 7}));
    CHECK((Ser{1.0, 1.0} * Ser{1.0, -1.0}).c == (std::array<double, 4>{1, 0, -1, 0}));
    CHECK((Ser{2.0} * Ser{1.5, 2.0, 3.0, 4.0}).c == (std::array<double, 4>{3, 4, 6, 8}));
    // (1 + t)^2 = 1 + 2t + t^2;  (1 + t)^-1 = 1 - t + t^2 - t^3;  (1 + t)^(1/2) = 1 + t/2 - t^2/8 + t^3/16;  (4 + 4t)^(-1/2) = (1/2)(1 - t/2 + 3t^2/8 - 5t^3/16)
    CHECK(fd::power_series(Ser{1.0, 1.0}, 2.0).c == (std::array<double, 4>{1, 2, 1, 0}));
    CHECK(fd::power_series(Ser{1.0, 1.0}, -1.0).c == (std::array<double, 4>{1, -1, 1, -1}));
    CHECK(fd::power_series(Ser{1.0, 1.0}, 0.5).c == (std::array<double, 4>{1, 0.5, -0.125, 0.0625}));
    CHECK(fd::power_series(Ser{4.0, 4.0}, -0.5).c == (std::array<double, 4>{0.5, -0.25, 0.1875, -0.15625}));
    // a series with higher terms: (1 + t + t^2)^2 = 1 + 2t + 3t^2 + 2t^3 + t^4
    CHECK(fd::power_series(Ser{1.0, 1.0, 1.0}, 2.0).c == (std::array<double, 4>{1, 2, 3, 2}));
}

TEST_CASE("a polynomial along a line is a series in t: the line x0 + t e, to the third power", "[forcemodel_fd_sizing][behaviour]") {
    // x^2 + 3 y z at x0 = (1, 2, 3), e = (1, 0, 1):  (1+t)^2 + 3 * 2 * (3 + t) = 19 + 8t + t^2
    gr::Poly a;
    a.set({2, 0, 0}, Rational(1));
    a.set({0, 1, 1}, Rational(3));
    CHECK(fd::poly_on_line(a, {1, 2, 3}, {1, 0, 1}).c == (std::array<double, 4>{19, 8, 1, 0}));
    // x^5 at (1,0,0) along x: (1+t)^5 = 1 + 5t + 10t^2 + 10t^3 + ...
    gr::Poly b;
    b.set({5, 0, 0}, Rational(1));
    CHECK(fd::poly_on_line(b, {1, 0, 0}, {1, 0, 0}).c == (std::array<double, 4>{1, 5, 10, 10}));
    // the constant, the zero polynomial, and a coefficient that is a fraction: x/4 at x0 = 2, e = 1: 1/2 + t/4
    gr::Poly c;
    c.set({0, 0, 0}, Rational(BigInt(7), BigInt(2)));
    CHECK(fd::poly_on_line(c, {5, 6, 7}, {1, 1, 1}).c == (std::array<double, 4>{3.5, 0, 0, 0}));
    CHECK(fd::poly_on_line(gr::Poly{}, {5, 6, 7}, {1, 1, 1}).c == (std::array<double, 4>{0, 0, 0, 0}));
    gr::Poly d;
    d.set({1, 0, 0}, Rational(BigInt(1), BigInt(4)));
    CHECK(fd::poly_on_line(d, {2, 0, 0}, {1, 0, 0}).c == (std::array<double, 4>{0.5, 0.25, 0, 0}));
}

TEST_CASE("the third derivative of the acceleration along a line through the origin is 24 GM e_i / rho^5, for the point mass", "[forcemodel_fd_sizing][behaviour]") {
    // a_i(t) = -GM e_i / (rho + t)^2 on the line x0 = rho e through the origin, e a unit vector; its third derivative at t = 0 is -GM e_i (-2)(-3)(-4) / rho^5 = 24 GM e_i / rho^5
    const std::vector<fd::FieldTerm> terms = fd::field_terms({});
    REQUIRE(terms.size() == 1);
    CHECK(terms[0].n == 0);
    CHECK(terms[0].s == 1);
    CHECK(terms[0].coef == fd::kGM);
    const double rho = 2.0;
    const std::array<double, 3> e = {0.6, 0.8, 0.0};
    const std::array<double, 3> x0 = {rho * e[0], rho * e[1], 0.0};
    const L gm = fd::kGM;
    CHECK(close(fd::third_derivative_a(terms, x0, e, 0), 24 * gm * static_cast<L>(0.6) / 32, 1e-14L));
    CHECK(close(fd::third_derivative_a(terms, x0, e, 1), 24 * gm * static_cast<L>(0.8) / 32, 1e-14L));
    CHECK(fd::third_derivative_a(terms, x0, e, 2) == 0.0);
    // the same through the point (0, 0, 3) along z: 24 GM / 3^5
    CHECK(close(fd::third_derivative_a(terms, {0.0, 0.0, 3.0}, {0.0, 0.0, 1.0}, 2), 24 * gm / 243, 1e-14L));
    // and the field with coefficients has the point mass first, then the coefficient terms: (2,0) has a C and no S, (2,1) has both, every line with a zero skipped
    // (a line of m = 0 has no S term whatever its S says, and a line of m = 1 has one when its S is not zero, even if its C is)
    const std::vector<fd::FieldTerm> richer = fd::field_terms({{2, 0, -4.8e-4, 0.0}, {2, 1, 1.0e-9, 0.0}, {2, 2, 2.0e-6, -1.4e-6}, {3, 0, 0.0, 0.0}, {3, 1, 0.0, 3.0e-7}, {4, 0, 1.0e-7, 5.0e-7}});
    REQUIRE(richer.size() == 1 + 1 + 1 + 2 + 1 + 1);   // point mass; (2,0,C); (2,1,C) (its S is 0); (2,2,C), (2,2,S); the (3,0) line has a zero C and no S; (3,1,S) (its C is 0); (4,0,C) (its S is no coefficient)
    CHECK(richer[5].n == 3);
    CHECK(richer[5].s == 7);
    CHECK(richer[5].q == gr::solid_polynomial(3, 1, 'S'));
    CHECK(close(richer[5].coef, fd::kGM * std::pow(6378136.3L, 3) * std::sqrt(static_cast<L>(7) / 6) * 3.0e-7L, 1e-13L));
    CHECK(richer[6].n == 4);
    CHECK(richer[6].q == gr::solid_polynomial(4, 0, 'C'));
    CHECK(richer[6].dq[0] == gr::p_der(gr::solid_polynomial(4, 0, 'C'), 0));
    CHECK(richer[6].dq[1] == gr::p_der(gr::solid_polynomial(4, 0, 'C'), 1));
    CHECK(richer[6].dq[2] == gr::p_der(gr::solid_polynomial(4, 0, 'C'), 2));
    CHECK(richer[4].dq[0] == gr::p_der(gr::solid_polynomial(2, 2, 'S'), 0));
    CHECK(richer[4].dq[1] == gr::p_der(gr::solid_polynomial(2, 2, 'S'), 1));
    CHECK(richer[1].n == 2);
    CHECK(richer[1].s == 5);
    CHECK(close(richer[1].coef, fd::kGM * std::pow(6378136.3L, 2) * std::sqrt(static_cast<L>(5)) * (-4.8e-4L), 1e-13L));
    CHECK(close(richer[2].coef, fd::kGM * std::pow(6378136.3L, 2) * std::sqrt(static_cast<L>(5) / 3) * 1.0e-9L, 1e-13L));
    CHECK(close(richer[3].coef, fd::kGM * std::pow(6378136.3L, 2) * std::sqrt(static_cast<L>(5) / 12) * 2.0e-6L, 1e-13L));
    CHECK(close(richer[4].coef, fd::kGM * std::pow(6378136.3L, 2) * std::sqrt(static_cast<L>(5) / 12) * (-1.4e-6L), 1e-13L));
    CHECK(richer[1].q == gr::solid_polynomial(2, 0, 'C'));
    CHECK(richer[4].q == gr::solid_polynomial(2, 2, 'S'));
    CHECK(richer[1].dq[2] == gr::p_der(gr::solid_polynomial(2, 0, 'C'), 2));
}

TEST_CASE("the scan on the grid of one position and two directions: the point mass, perpendicular to the radius, where d^3 a_y / dt^3 is 9 GM / r^5", "[forcemodel_fd_sizing][behaviour]") {
    // grid 1 x 1 x 1: the position on the equator at longitude pi (x = (-r, 0, 0)), the directions e = (0, +-1, 0) and the stencil -h, 0, +h along them.  On the line x = (-r, s, 0), a_y = -GM s (r^2 + s^2)^(-3/2),
    // whose third derivative is GM (9 r^4 - 72 r^2 s^2 + 24 s^4) (r^2 + s^2)^(-9/2): 9 GM / r^5 at the centre of the stencil, and smaller at +-h (by 12.5 h^2 / r^2); a_x is even in s: its third
    // derivative is 0 at the centre and tiny at +-h.  So the supremum is 9 GM / r^5.
    const double r = 6678136.3;
    const L want = 9 * static_cast<L>(fd::kGM) / lpow(static_cast<L>(r), 5);
    CHECK(close(fd::scan(0, r, {}, 1, 1, 1), want, 1e-12L));
    // F is a bound of the true supremum, whatever grid is looked at
    const double bound = fd::f_gravity(4, r - 1000.0, {});
    CHECK(fd::scan(0, r, {}, 3, 4, 3) <= bound);
    CHECK(fd::scan(0, r, {}, 3, 4, 3) > 0.1 * bound);
    CHECK(fd::scan(0, r, {}) <= bound);
    // the default grid is 5 x 6 x 6 (and twice as many azimuths), and no grid is the empty one
    CHECK(fd::scan(0, r, {}) == fd::scan(0, r, {}, 5, 6, 6));
    CHECK(fd::scan(0, r, {}, 0, 6, 6) == 0.0);
}

TEST_CASE("py_sum3 is CPython's sum() of three floats: the first added to the int 0, the others compensated by Neumaier's method, the compensation added at the end unless it is not finite", "[forcemodel_fd_sizing][behaviour]") {
    CHECK(fd::py_sum3(1.0, 2.0, 3.0) == 6.0);
    CHECK(fd::py_sum3(0.5, -0.25, 0.125) == 0.375);
    // what each addition loses is found again at the end.  Above 2^53 the spacing is 2: 2^53 + 2 plus two halves is 2^53 + 3, a tie between 2^53 + 2 and 2^53 + 4, which goes to the even one, 2^53 + 4;
    // added one by one the halves are lost, and the sum stays 2^53 + 2
    CHECK(fd::py_sum3(9007199254740994.0, 0.5, 0.5) == 9007199254740996.0);
    CHECK((9007199254740994.0 + 0.5) + 0.5 == 9007199254740994.0);
    // 1e16 + 1 + 1: the exact sum 1e16 + 2 is a double, and neither addition alone moves 1e16; the compensation does, whichever of the three is the large one
    CHECK(fd::py_sum3(1e16, 1.0, 1.0) == 10000000000000002.0);
    CHECK(fd::py_sum3(1.0, 1e16, 1.0) == 10000000000000002.0);
    CHECK(fd::py_sum3(1.0, 1.0, 1e16) == 10000000000000002.0);
    CHECK(fd::py_sum3(-1e16, 1.0, 1.0) == -9999999999999998.0);
    CHECK(fd::py_sum3(1e16, -1.0, -1.0) == 9999999999999998.0);
    // the same where the parts that are lost are not integers: above 2^49 the spacing is 0.125, so 0.05 is lost twice, their sum 0.1 is found again, and it rounds to the next double
    CHECK(fd::py_sum3(562949953421312.0, 0.05, 0.05) == 562949953421312.125);
    // a compensation that is not finite is left out: 1e308 + 1e308 overflows, and the sum is the infinity that CPython's is
    const double overflowed = fd::py_sum3(1e308, 1e308, -1e308);
    CHECK(std::isinf(overflowed));
    CHECK(overflowed > 0);
    const double underflowed = fd::py_sum3(-1e308, -1e308, 1e308);
    CHECK(std::isinf(underflowed));
    CHECK(underflowed < 0);
    CHECK(std::isnan(fd::py_sum3(std::nan(""), 1.0, 2.0)));
}

TEST_CASE("max_l1_gradient: the largest of the three l1 norms of a polynomial's derivatives, whichever axis has it", "[forcemodel_fd_sizing][behaviour]") {
    const auto linear = [](int a, int b, int c) {
        gr::Poly p;
        p.set({1, 0, 0}, Rational(a));
        p.set({0, 1, 0}, Rational(b));
        p.set({0, 0, 1}, Rational(c));
        return p;
    };
    CHECK(fd::max_l1_gradient(linear(5, 1, 2)) == Rational(5));
    CHECK(fd::max_l1_gradient(linear(1, 5, 2)) == Rational(5));
    CHECK(fd::max_l1_gradient(linear(1, 2, 5)) == Rational(5));
    CHECK(fd::max_l1_gradient(linear(-7, 2, 3)) == Rational(7));
    // x^2 + 3xy: d/dx = 2x + 3y (l1 norm 5), d/dy = 3x (3), d/dz = 0
    gr::Poly q;
    q.set({2, 0, 0}, Rational(1));
    q.set({1, 1, 0}, Rational(3));
    CHECK(fd::max_l1_gradient(q) == Rational(5));
    CHECK(fd::max_l1_gradient(gr::Poly{}) == Rational(0));
}

TEST_CASE("the third derivative of the acceleration of a field with a J2-like term, against a finite difference of the field's own acceleration written out in closed form", "[forcemodel_fd_sizing][behaviour]") {
    // V = GM / r + K (2 z^2 - x^2 - y^2) / (2 r^5), K = GM a_e^2 sqrt(5) C20, a_i = dV / dx_i; C20 is chosen far too large, so that the term is a few percent of the field and any slip in it shows.  The third
    // derivative along e of a_i(x0 + t e) is the five-point difference (f(2h) - 2 f(h) + 2 f(-h) - f(-2h)) / (2 h^3), whose error is of the order of (h / r)^2
    const L c20 = -0.1L;
    const L gm = fd::kGM;
    const L k = gm * lpow(static_cast<L>(fd::kAE), 2) * std::sqrt(static_cast<L>(5)) * c20;
    const auto acceleration = [&](int i, const std::array<L, 3>& x) {
        const L r2 = x[0] * x[0] + x[1] * x[1] + x[2] * x[2];
        const L r3 = r2 * std::sqrt(r2);
        const L r5 = r3 * r2;
        const L r7 = r5 * r2;
        const L q = 2 * x[2] * x[2] - x[0] * x[0] - x[1] * x[1];
        const std::array<L, 3> dq = {-2 * x[0], -2 * x[1], 4 * x[2]};
        const auto at = static_cast<std::size_t>(i);
        return -gm * x[at] / r3 + k * (dq[at] / (2 * r5) - 5 * x[at] * q / (2 * r7));
    };
    const std::vector<fd::FieldTerm> terms = fd::field_terms({{2, 0, -0.1, 0.0}});
    REQUIRE(terms.size() == 2);
    const std::vector<std::pair<std::array<double, 3>, std::array<double, 3>>> lines = {
        {{3.0e6, -2.0e6, 5.0e6}, {0.36, -0.48, 0.8}}, {{-4.0e6, 1.0e6, 3.0e6}, {0.0, 1.0, 0.0}}, {{1.0e6, 5.0e6, -4.0e6}, {1.0, 0.0, 0.0}}, {{2.0e6, 3.0e6, 6.0e6}, {0.0, 0.0, 1.0}}};
    const L h = 6.0e3L;
    for (const auto& [x0, e] : lines) {
        for (int i = 0; i < 3; ++i) {
            const auto f = [&](L t) { return acceleration(i, {x0[0] + t * e[0], x0[1] + t * e[1], x0[2] + t * e[2]}); };
            const L want = (f(2 * h) - 2 * f(h) + 2 * f(-h) - f(-2 * h)) / (2 * h * h * h);
            INFO("x0 (" << x0[0] << ", " << x0[1] << ", " << x0[2] << ") e (" << e[0] << ", " << e[1] << ", " << e[2] << ") component " << i);
            CHECK(close(fd::third_derivative_a(terms, x0, e, i), want, 3e-4L));
        }
    }
}

TEST_CASE("scan: the supremum over the grid its documentation gives, written out again, for a field that is not symmetric, so that every shift of the grid shows", "[forcemodel_fd_sizing][behaviour]") {
    const std::vector<fd::Coefficient> coefs = {{2, 1, 0.06, -0.07}, {3, 2, -0.05, 0.04}};   // far larger than the Earth's, so that the terms are a few percent of the field
    const std::vector<fd::FieldTerm> terms = fd::field_terms(coefs);
    const double r = 7.0e6;
    constexpr double pi = std::numbers::pi;
    for (const std::array<int, 3>& grid : std::vector<std::array<int, 3>>{{1, 1, 1}, {2, 3, 2}, {3, 2, 1}, {1, 4, 3}, {4, 1, 2}}) {
        // latitudes, longitudes and polar angles at the middles of equal cells, azimuths of the same width over twice the range, and the stencil -h, 0, +h along each direction
        double want = 0.0;
        for (int ia = 0; ia < grid[0]; ++ia) {
            const double lat = -pi / 2 + pi * (ia + 0.5) / grid[0];
            for (int ib = 0; ib < grid[1]; ++ib) {
                const double lon = 2 * pi * (ib + 0.5) / grid[1];
                const std::array<double, 3> x = {r * std::cos(lat) * std::cos(lon), r * std::cos(lat) * std::sin(lon), r * std::sin(lat)};
                for (int ic = 0; ic < grid[2]; ++ic) {
                    const double th = pi * (ic + 0.5) / grid[2];
                    for (int idr = 0; idr < 2 * grid[2]; ++idr) {
                        const double ph = pi * (idr + 0.5) / grid[2];
                        const std::array<double, 3> e = {std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)};
                        for (const double step : {-fd::kHMax, 0.0, fd::kHMax}) {
                            const std::array<double, 3> x0 = {x[0] + step * e[0], x[1] + step * e[1], x[2] + step * e[2]};
                            for (int i = 0; i < 3; ++i) want = std::max(want, std::fabs(fd::third_derivative_a(terms, x0, e, i)));
                        }
                    }
                }
            }
        }
        INFO("grid " << grid[0] << " x " << grid[1] << " x " << grid[2]);
        CHECK(want > 0.0);
        CHECK(close(fd::scan(0, r, coefs, grid[0], grid[1], grid[2]), want, 1e-12L));
    }
}

// ======================================================================================================================================== the tool

TEST_CASE("--check prints the eight frozen lines and then the sizing; on a tree with a coefficient file of four lines the three sections that need none are exactly the ones below", "[forcemodel_fd_sizing][behaviour]") {
    Tree t(kEgmHead);
    const Result r = run_on(t.settings(), {"--check"});
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(starts_with(r.out,
                      "ok       S3(1,3): 96\n"
                      "ok       S3(1,4): 180\n"
                      "ok       S3(2,5): 420\n"
                      "ok       S3(0,3): 60\n"
                      "ok       S3(3,7): 1140\n"
                      "ok       S3(1,5): 300\n"
                      "ok       Phi_20: 26832.815729997477\n"
                      "ok       Phi_point_mass: 96.0\n"
                      "\nGravity(N = 4): rigorous F at r_min = r - 1000 m, the registered nu = 64 eps |a|, eps(h), and the best relative defect it can see\n"
                      "  point                   r (m)    F (m^-2 s^-2)   nu (m s^-2)    eps(10m)    eps(30m)   eps(100m)   eps(300m)  eps(1000m)   min eps / |G|\n"
                      "  GPS                26561000.0 "));
    CHECK(contains(r.out, "\nGravity(N = 4): the three wrong rows' predicted power, max |wrong - right| / (eps at the best step); the registered requirement is >= 1e3\n"));
    CHECK(contains(r.out, "\nThird body (mu / d_min^5 x 96), the Sun and the Moon at a LEO point:\n"));
    CHECK(contains(r.out, "\nRelativity at the LEO 300 km point (v = sqrt(GM/r)):\n"));
    CHECK_FALSE(contains(r.out, "Looseness"));
    // the sections that do not read the file: the third body and the relativity, from the docstring's formulas written out again (here: the Sun's F, nu and |G|)
    {
        const L mu = 1.32712442099e20L, s = 1.4959787e11L;
        const L f = mu * 96 / lpow(s - 7.0e6L - 1000, 5);
        const L nu = 16 * 0x1p-52L * mu * (1 / (s * s) + 1 / (s * s));
        const L g = 2 * mu / (s * s * s);
        std::array<char, 400> line{};
        std::snprintf(line.data(), line.size(), "  Sun   F = %.3Le  nu = %.3Le  |G| ~ %.3Le  best eps = ", f, nu, g);
        CHECK(contains(r.out, line.data()));
    }
    // the two files agree on the sizing: without --check the output is the same, and with --scan the scan follows
    const Result plain = run_on(t.settings(), {});
    CHECK(plain.code == kOk);
    CHECK(plain.out == r.out);
}

TEST_CASE("--scan adds the looseness of F against the true supremum, four lines, each with F / true sup of at least one (F is a bound)", "[forcemodel_fd_sizing][behaviour]") {
    Tree t(kEgmHead);
    const Result plain = run_on(t.settings(), {});
    const Result scanned = run_on(t.settings(), {"--scan"});
    CHECK(scanned.code == kOk);
    REQUIRE(starts_with(scanned.out, plain.out));
    const std::string tail = scanned.out.substr(plain.out.size());
    CHECK(starts_with(tail, "\nLooseness, the TRUE supremum of |d_e^3 a_i| against F (point mass + degrees 2..4 of EGM2008; coarse grid, exact series):\n  GPS              true sup "));
    int rows = 0;
    for (std::size_t at = tail.find("F / true sup = "); at != std::string::npos; at = tail.find("F / true sup = ", at + 1)) {
        ++rows;
        const double ratio = std::strtod(tail.c_str() + at + 15, nullptr);
        CHECK(ratio >= 1.0);
        CHECK(ratio < 100.0);
    }
    CHECK(rows == 4);
    CHECK(contains(tail, "\n  LEO 300 km       true sup "));
    CHECK(contains(tail, "\n  LEO 952.86 km    true sup "));
    CHECK(contains(tail, "\n  sail 720 km      true sup "));
}

TEST_CASE("a frozen number that is not reproduced is MISMATCH with the frozen value beside it, and with --check the exit status is 1 (without, the line is printed and the status is 0)", "[forcemodel_fd_sizing][behaviour]") {
    Tree t(kEgmHead);
    fd::Settings s = t.settings();
    s.s3[0] = {1, 3, 97};
    const Result checked = run_on(s, {"--check"});
    CHECK(checked.code == kFailed);
    CHECK(starts_with(checked.out, "MISMATCH S3(1,3): 96  (frozen 97)\nok       S3(1,4): 180\n"));
    const Result unchecked = run_on(s, {});
    CHECK(unchecked.code == kOk);
    CHECK(unchecked.out == checked.out);
    // Phi_20 is compared to 1e-12 relative: 12001 in place of 12000 is far outside, and the frozen figure is printed as the Python's repr
    fd::Settings p = t.settings();
    p.phi20_scale = 12001.0;
    const Result phi = run_on(p, {"--check"});
    CHECK(phi.code == kFailed);
    CHECK(contains(phi.out, "MISMATCH Phi_20: 26832.815729997477  (frozen 26835.05"));
    // a floor on the power of the controls that they do not reach: the check fails (the Python kept the names of the twelve and printed none of them), and the printed table is unchanged
    fd::Settings f = t.settings();
    f.power_floor = 1e30;
    const Result floor = run_on(f, {"--check"});
    CHECK(floor.code == kFailed);
    const Result base = run_on(t.settings(), {"--check"});
    CHECK(floor.out == base.out);
    CHECK(run_on(f, {}).code == kOk);
}

TEST_CASE("a coefficient file that is not there is exit 2 after the eight lines that need none; an unreadable or malformed one is exit 2, naming it", "[forcemodel_fd_sizing][behaviour]") {
    TempDir dir("odl-fd");
    fd::Settings s = fd::default_settings();
    s.root = dir.path();
    const Result missing = run_on(s, {"--check"});
    CHECK(missing.code == kArgument);
    CHECK(starts_with(missing.out, "ok       S3(1,3): 96\n"));
    CHECK(missing.out.find("Phi_point_mass: 96.0\n") != std::string::npos);
    CHECK_FALSE(contains(missing.out, "Gravity(N = 4)"));
    CHECK(missing.err == "forcemodel_fd_sizing: error: the coefficient file does not exist: " + (dir.path() / kEgmRelative).string() + "\n");
    // the root given on the command line wins over the settings'
    Tree t(kEgmHead);
    CHECK(run_on(s, {"--root", t.root.string()}).code == kOk);
    CHECK(run_on(s, {"--root=" + t.root.string()}).code == kOk);
    // a directory where the file should be, and a file with a line that is none
    fs::create_directories(dir.path() / kEgmRelative);
    const Result directory = run_on(s, {});
    CHECK(directory.code == kArgument);
    CHECK(contains(directory.err, "is not a regular file"));
    Tree bad("    2    0   oops   0.0\n");
    const Result malformed = run_on(bad.settings(), {});
    CHECK(malformed.code == kArgument);
    CHECK(malformed.err == "forcemodel_fd_sizing: error: " + (bad.root / kEgmRelative).string() + ": line 1 is not a coefficient line\n");
}

TEST_CASE("the command line: -h prints the usage and the options; an option that is not one, or without its argument, is an error of exit status 2", "[forcemodel_fd_sizing][behaviour]") {
    Tree t(kEgmHead);
    const std::string usage = "usage: forcemodel_fd_sizing [-h] [--scan] [--check] [--root ROOT]\n";
    static const char kHelp[] = R"HELP(usage: forcemodel_fd_sizing [-h] [--scan] [--check] [--root ROOT]

The finite-difference gate of the force plugins' Jacobians: its sizing, as the numbers it freezes (SPEC-forcemodel.md §6.2): the bound F of the third derivative of each force over the whole
stencil, the registered noise bound nu, eps(h) = h^2 F / 6 + nu / h, and the power of the gate against its three wrong rows.

options:
  -h, --help   show this help and exit
  --scan       also run the looseness scan: the true supremum of |d_e^3 a_i| by exact power series, against F
  --check      every frozen number reproduced (exit 1 if one is not)
  --root ROOT  the tree whose data/cache holds the coefficient file (default: the tree this tool was built from)

exit codes: 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced
            2 an argument error, or the coefficient file is missing or unreadable
)HELP";   // the text of the tool's own help, pinned
    for (const char* flag : {"-h", "--help"}) {
        const Result help = run_on(t.settings(), {flag});
        CHECK(help.code == kOk);
        CHECK(starts_with(help.out, usage));
        CHECK(help.out == kHelp);
        CHECK(contains(help.out, "  --scan       also run the looseness scan: the true supremum of |d_e^3 a_i| by exact power series, against F\n"));
        CHECK(contains(help.out, "  --check      every frozen number reproduced (exit 1 if one is not)\n"));
        CHECK(contains(help.out, "  --root ROOT  the tree whose data/cache holds the coefficient file (default: the tree this tool was built from)\n"));
        CHECK(contains(help.out, "exit codes: 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced\n"));
        CHECK(help.err.empty());
    }
    const Result unknown = run_on(t.settings(), {"--frobnicate"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == usage + "forcemodel_fd_sizing: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    CHECK(run_on(t.settings(), {"stray"}).code == kArgument);
    const Result bare = run_on(t.settings(), {"--root"});
    CHECK(bare.code == kArgument);
    CHECK(bare.err == usage + "forcemodel_fd_sizing: error: argument --root: expected one argument\n");
    const Result swallowed = run_on(t.settings(), {"--root", "--check"});
    CHECK(swallowed.code == kArgument);
    CHECK(swallowed.err == usage + "forcemodel_fd_sizing: error: argument --root: expected one argument\n");
    // a word that begins with two dashes is an option, however many more follow: `---x` is not a root either
    const Result dashes = run_on(t.settings(), {"--root", "---x"});
    CHECK(dashes.code == kArgument);
    CHECK(dashes.err == usage + "forcemodel_fd_sizing: error: argument --root: expected one argument\n");
    CHECK(run_on(t.settings(), {"--scan=1"}).code == kArgument);
    CHECK(run_on(t.settings(), {"--check=1"}).code == kArgument);
    CHECK(run_on(t.settings(), {"-h", "--frobnicate"}).code == kOk);
    CHECK(run_on(t.settings(), {"--frobnicate", "-h"}).code == kArgument);
    // the default settings name the tree this tool was built from
    CHECK(fd::default_settings().root == fd::default_root());
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[forcemodel_fd_sizing][behaviour]") {
    Tree t(kEgmHead);
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(fd::run_on(t.settings(), {}, Streams{out, err}) == kInternal);
    CHECK(err.str() == "forcemodel_fd_sizing: internal error: boom\n");
}

// ======================================================================================================================================== the real tree

TEST_CASE("on the real coefficient file --check prints, byte for byte, what the Python tool recorded (forcemodel_fd_sizing_check.out), and --scan the rest (forcemodel_fd_sizing_scan.out)", "[forcemodel_fd_sizing][real_tree]") {
    const std::string recorded = kRecordedScan;
    REQUIRE(recorded.size() == 2502);
    const Result checked = run_real({"--check"});
    CHECK(checked.code == kOk);
    CHECK(checked.err.empty());
    CHECK(checked.out == recorded.substr(0, kRecordedCheckBytes));
    const Result scanned = run_real({"--scan"});
    CHECK(scanned.code == kOk);
    CHECK(scanned.err.empty());
    CHECK(scanned.out == recorded);
}

TEST_CASE("the committed comparator header's Phi are the ones the sizing computes", "[forcemodel_fd_sizing][real_tree]") {
    // modules/forcemodel/tests/comparator_polynomials.hpp carries Phi_nm of every coefficient of degrees 2 - 4 in hexadecimal; the first three rows and the (4,0) row, derived above or written there
    // (a hexadecimal float is the double itself: no text to round)
    CHECK(fd::phi(2, 0, 'C') == 0x1.a343434eb9762p+14);
    CHECK(fd::phi(2, 1, 'C') == 0x1.6b1799add9640p+14);
    CHECK(fd::phi(2, 2, 'C') == 0x1.6b1799add9640p+14);
    CHECK(fd::phi(3, 0, 'C') == 0x1.7934a1a03c0c8p+17);
    CHECK(fd::phi(4, 0, 'C') == 0x1.0e7e000000000p+20);
}
