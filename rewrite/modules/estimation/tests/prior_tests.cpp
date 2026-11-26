// prior_tests.cpp — SPEC-estimation.md §8.4: step 4's acceptance tests EST-A-401 .. EST-A-407 (the a priori constraints, isotropic and RTN), REGISTERED on 2026-11-27 (v0.5, commit
// d2df6aa) BEFORE this module's code existed.  Every construction, expected value, tolerance and control below is the registered one; a figure the specification calls *predicted* is a closed
// form computed from the stated construction.  **Nothing here may be tuned to a result**: a run that misses is reported with the criterion as written.
//
// The constants this file fixes that §8.4 left to the test header (executor-classified, TREE-06): the seed 20261127; the state (r_A, v_A) = ((6878.137, 0, 0) km, (0, 5.381, 5.381) km/s); the data of
// EST-A-405; the decade list of EST-A-402; the integer case of EST-A-401 (b); the rotation of EST-A-403 (b); the test-level tolerances where §8.4 says "to B-relative" or "within the sum of the two
// certified errors" (each stated at its place).
//
// INDEPENDENT REFERENCES, never the module's own arithmetic: the stacked whitened system [A; L] solved by Householder QR in `long double`; exact integer (Cramer) arithmetic; closed forms.
// MODULE-FREE: this file links `estimation` and `core` only (the module is `core`-only, EST-Q-008); the RTN basis is built here by the formula of FRAME-R-040, and its agreement with
// frames::rtn_basis is tests/l7_prior/prior_frames_tests.cpp (EST-A-403 d).

#include <odl/estimation/levenberg_marquardt.hpp>
#include <odl/estimation/normal_equations.hpp>
#include <odl/estimation/prior.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace odl::estimation;
using ld = long double;

namespace {

constexpr std::uint64_t kSeed = 20261127ULL;
constexpr double kU = 0x1p-53;

// ----------------------------------------------------------------------------------------------------------- the generator (splitmix64), uniform and the polar-form normal
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        s += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uni() { return static_cast<double>(next() >> 11) * 0x1p-52 - 1.0; }          // [-1, 1)
    double normal() {                                                                      // the polar form
        for (;;) {
            const double u = uni(), v = uni();
            const double q = u * u + v * v;
            if (q >= 1.0 || q == 0.0) continue;
            return u * std::sqrt(-2.0 * std::log(q) / q);
        }
    }
};

// ----------------------------------------------------------------------------------------------------------- the RTN basis by the formula of FRAME-R-040 (plain double, no `frames`)
std::array<double, 9> rtn_formula(const std::array<double, 3>& r, const std::array<double, 3>& v) {
    const double rn = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    const std::array<double, 3> h{r[1] * v[2] - r[2] * v[1], r[2] * v[0] - r[0] * v[2], r[0] * v[1] - r[1] * v[0]};
    const double hn = std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
    std::array<double, 3> eR{r[0] / rn, r[1] / rn, r[2] / rn}, eN{h[0] / hn, h[1] / hn, h[2] / hn};
    const std::array<double, 3> eT{eN[1] * eR[2] - eN[2] * eR[1], eN[2] * eR[0] - eN[0] * eR[2], eN[0] * eR[1] - eN[1] * eR[0]};
    return {eR[0], eR[1], eR[2], eT[0], eT[1], eT[2], eN[0], eN[1], eN[2]};
}

std::array<double, 9> fixture_basis() { return rtn_formula({6878.137, 0.0, 0.0}, {0.0, 5.381, 5.381}); }

std::array<double, 9> transposed(const std::array<double, 9>& B) {
    std::array<double, 9> t{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) t[3 * i + j] = B[3 * j + i];
    return t;
}

// ----------------------------------------------------------------------------------------------------------- long double dense helpers (INDEPENDENT of the module)
using Mat = std::vector<ld>;   // row-major

// least squares min ||S c - t|| by Householder QR, long double
std::vector<ld> qr_solve(Mat S, std::vector<ld> t, std::size_t m, std::size_t n) {
    for (std::size_t j = 0; j < n; ++j) {
        ld nrm = 0.0L;
        for (std::size_t i = j; i < m; ++i) nrm += S[i * n + j] * S[i * n + j];
        nrm = std::sqrt(nrm);
        if (nrm == 0.0L) continue;
        const ld alpha = S[j * n + j] > 0.0L ? -nrm : nrm;
        std::vector<ld> v(m, 0.0L);
        for (std::size_t i = j; i < m; ++i) v[i] = S[i * n + j];
        v[j] -= alpha;
        ld vv = 0.0L;
        for (std::size_t i = j; i < m; ++i) vv += v[i] * v[i];
        if (vv == 0.0L) continue;
        for (std::size_t c = j; c < n; ++c) {
            ld dot = 0.0L;
            for (std::size_t i = j; i < m; ++i) dot += v[i] * S[i * n + c];
            const ld f = 2.0L * dot / vv;
            for (std::size_t i = j; i < m; ++i) S[i * n + c] -= f * v[i];
        }
        ld dot = 0.0L;
        for (std::size_t i = j; i < m; ++i) dot += v[i] * t[i];
        const ld f = 2.0L * dot / vv;
        for (std::size_t i = j; i < m; ++i) t[i] -= f * v[i];
    }
    std::vector<ld> c(n, 0.0L);
    for (std::size_t jj = n; jj-- > 0;) {
        ld s = t[jj];
        for (std::size_t k = jj + 1; k < n; ++k) s -= S[jj * n + k] * c[k];
        c[jj] = s / S[jj * n + jj];
    }
    return c;
}

// inverse of a square matrix by Gauss-Jordan with partial pivoting, long double
Mat inverse(Mat a, std::size_t n) {
    Mat inv(n * n, 0.0L);
    for (std::size_t i = 0; i < n; ++i) inv[i * n + i] = 1.0L;
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t p = c;
        for (std::size_t i = c + 1; i < n; ++i)
            if (std::fabs(a[i * n + c]) > std::fabs(a[p * n + c])) p = i;
        for (std::size_t j = 0; j < n; ++j) {
            std::swap(a[c * n + j], a[p * n + j]);
            std::swap(inv[c * n + j], inv[p * n + j]);
        }
        const ld d = a[c * n + c];
        for (std::size_t j = 0; j < n; ++j) {
            a[c * n + j] /= d;
            inv[c * n + j] /= d;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == c) continue;
            const ld f = a[i * n + c];
            if (f == 0.0L) continue;
            for (std::size_t j = 0; j < n; ++j) {
                a[i * n + j] -= f * a[c * n + j];
                inv[i * n + j] -= f * inv[c * n + j];
            }
        }
    }
    return inv;
}

// ----------------------------------------------------------------------------------------------------------- a data set (rows, residuals, sigmas) and its handling
struct Data {
    std::size_t n = 0, m = 0;
    std::vector<double> A;        // m x n
    std::vector<double> r;        // residuals (observed - computed), m
    std::vector<double> sigma;    // m
};

std::vector<std::string> names_of(std::size_t n, const char* prefix) {
    std::vector<std::string> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back(prefix + std::to_string(i));
    return v;
}

NormalEquations make_ne(const std::vector<std::string>& names, const Data& d) {
    auto made = NormalEquations::create(names);
    REQUIRE(made.has_value());
    NormalEquations ne = std::move(*made);
    for (std::size_t k = 0; k < d.m; ++k) {
        const auto ok = ne.add(std::span<const double>(&d.A[k * d.n], d.n), d.r[k], d.sigma[k]);
        REQUIRE(ok.has_value());
    }
    return ne;
}

// the prior block's whitened rows by the DEFINITION, in long double (the reference's own code): rows over `n` parameters, `at` the parameter indices of the block's names
struct RefRows {
    Mat rows;                    // r x n
    std::vector<ld> res;         // r
};
RefRows ref_prior(const PriorBlock& b, const std::vector<std::size_t>& at, std::size_t n, const std::vector<double>& x_lin) {
    RefRows out;
    const std::size_t r = b.basis ? 3 : b.names.size();
    out.rows.assign(r * n, 0.0L);
    out.res.assign(r, 0.0L);
    std::vector<ld> d(b.names.size());
    for (std::size_t j = 0; j < d.size(); ++j) d[j] = static_cast<ld>(b.centre[j]) - static_cast<ld>(x_lin[at[j]]);
    if (!b.basis) {
        for (std::size_t i = 0; i < r; ++i) {
            out.rows[i * n + at[i]] = 1.0L / static_cast<ld>(b.sigma[i]);
            out.res[i] = d[i] / static_cast<ld>(b.sigma[i]);
        }
    } else {
        const auto& B = *b.basis;
        for (std::size_t k = 0; k < 3; ++k) {
            ld acc = 0.0L;
            for (std::size_t j = 0; j < 3; ++j) {
                out.rows[k * n + at[j]] = static_cast<ld>(B[3 * k + j]) / static_cast<ld>(b.sigma[k]);
                acc += static_cast<ld>(B[3 * k + j]) * d[j];
            }
            out.res[k] = acc / static_cast<ld>(b.sigma[k]);
        }
    }
    return out;
}

struct Reference {
    std::vector<ld> x;
    ld rss_total = 0.0L;
    std::size_t rows = 0;
};
// the stacked whitened system [A_w; L] c = [r_w; d_w] solved by QR in long double
Reference reference_solve(const Data& d, const std::vector<RefRows>& priors) {
    std::size_t rows = d.m;
    for (const auto& p : priors) rows += p.res.size();
    Mat S(rows * d.n, 0.0L);
    std::vector<ld> t(rows, 0.0L);
    for (std::size_t k = 0; k < d.m; ++k) {
        for (std::size_t j = 0; j < d.n; ++j) S[k * d.n + j] = static_cast<ld>(d.A[k * d.n + j]) / static_cast<ld>(d.sigma[k]);
        t[k] = static_cast<ld>(d.r[k]) / static_cast<ld>(d.sigma[k]);
    }
    std::size_t at = d.m;
    for (const auto& p : priors)
        for (std::size_t k = 0; k < p.res.size(); ++k, ++at) {
            for (std::size_t j = 0; j < d.n; ++j) S[at * d.n + j] = p.rows[k * d.n + j];
            t[at] = p.res[k];
        }
    Reference ref;
    ref.x = qr_solve(S, t, rows, d.n);
    ref.rows = rows;
    for (std::size_t k = 0; k < rows; ++k) {
        ld s = t[k];
        for (std::size_t j = 0; j < d.n; ++j) s -= S[k * d.n + j] * ref.x[j];
        ref.rss_total += s * s;
    }
    return ref;
}

// the scaled error phi(xhat, xref) = ||D (xhat - xref)|| / ||D xhat||, D = diag(sqrt(N_ii)) of `ne`'s formed system
double phi_of(const NormalEquations& ne, const std::vector<double>& xhat, const std::vector<ld>& xref) {
    const auto fs = ne.formed_system();
    REQUIRE(fs.has_value());
    ld num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < fs->n; ++i) {
        const ld Nii = static_cast<ld>(fs->matrix[i * fs->n + i]);
        const ld e = static_cast<ld>(xhat[i]) - xref[i];
        num += Nii * e * e;
        den += Nii * static_cast<ld>(xhat[i]) * static_cast<ld>(xhat[i]);
    }
    return static_cast<double>(std::sqrt(num / den));
}

double dnorm_of(const NormalEquations& ne, const std::vector<double>& x) {
    const auto fs = ne.formed_system();
    REQUIRE(fs.has_value());
    ld s = 0.0L;
    for (std::size_t i = 0; i < fs->n; ++i) s += static_cast<ld>(fs->matrix[i * fs->n + i]) * static_cast<ld>(x[i]) * static_cast<ld>(x[i]);
    return static_cast<double>(std::sqrt(s));
}

#define SAY(format, ...) std::printf("PRIOR " format "\n", __VA_ARGS__)

// ----------------------------------------------------------------------------------------------------------- the problem P1 of EST-A-401 and its prior blocks
constexpr double kColumnScale[6] = {1.0, 1.0e3 /* NOT-A-UNIT-CROSSING: a column's unit, EST-A-401's stress on the scaling */, 1.0e-3 /* NOT-A-UNIT-CROSSING: the same */, 1.0, 1.0, 1.0};

Data make_p1(Rng& g) {
    Data d;
    d.n = 6;
    d.m = 40;
    d.A.resize(d.n * d.m);
    d.r.resize(d.m);
    d.sigma.resize(d.m);
    const double truth[6] = {1.0, -0.5, 2.0, 0.25, -1.0, 0.5};
    for (std::size_t k = 0; k < d.m; ++k) {
        d.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);                                  // [0.5, 2)
        double acc = 0.0;
        for (std::size_t j = 0; j < d.n; ++j) {
            d.A[k * d.n + j] = g.uni() * kColumnScale[j];
            acc += d.A[k * d.n + j] * (truth[j] / kColumnScale[j]);
        }
        d.r[k] = acc + 0.3 * d.sigma[k] * g.normal();
    }
    return d;
}

PriorBlock p1_iso(double sign = 1.0, double sigma0_factor = 1.0) {
    auto b = PriorBlock::isotropic({"a0", "a1", "a2"}, {sign * 1.0, sign * -1.0, sign * 0.5}, {0.5 * sigma0_factor, 2.0, 8.0});
    REQUIRE(b.has_value());
    return *b;
}
PriorBlock p1_rtn(const std::array<double, 9>& basis) {
    auto b = PriorBlock::rtn({"a3", "a4", "a5"}, {1.0, -1.0, 0.5}, basis, {0.2, 200.0, 0.2});
    REQUIRE(b.has_value());
    return *b;
}

const std::vector<std::string> kP1Names = {"a0", "a1", "a2", "a3", "a4", "a5"};

}  // namespace

// =====================================================================================================================================================================================
TEST_CASE("EST-A-401: the constrained solve is the stacked least-squares solution (isotropic + RTN blocks), exactly, with controls that fail", "[estimation][prior][a401]") {
    Rng g(kSeed);
    const Data d = make_p1(g);
    const std::vector<double> x_lin(6, 0.0);
    const std::array<double, 9> B = fixture_basis();
    const PriorBlock iso = p1_iso(), rtn = p1_rtn(B);

    auto ne = make_ne(kP1Names, d);
    REQUIRE(ne.add_prior(iso, x_lin).has_value());
    REQUIRE(ne.add_prior(rtn, x_lin).has_value());
    const auto sol = ne.solve();
    REQUIRE(sol.has_value());
    const double Bcert = sol->certified_relative_error;

    const std::vector<std::size_t> at_iso{0, 1, 2}, at_rtn{3, 4, 5};
    const Reference ref = reference_solve(d, {ref_prior(iso, at_iso, 6, x_lin), ref_prior(rtn, at_rtn, 6, x_lin)});
    REQUIRE(ref.rows == 46);

    // (a) the reference: phi <= B + the reference's own error 46 * 2^-64 * kappa_up
    const double phi = phi_of(ne, sol->x, ref.x);
    const double ref_err = 46.0 * 0x1p-64 * sol->condition.condition_upper;
    SAY("401a phi %.3e  B %.3e  reference error bound %.3e  kappa_up %.3e", phi, Bcert, ref_err, sol->condition.condition_upper);
    CHECK(phi <= Bcert + ref_err);

    // (c) the counts: nu = m + r - n_free = 40, r = 6; Omega_data + Omega_prior equals the reference's, to 16 B (Omega + ||D x||^2) (test-level constant: "B-relative", executor-classified)
    CHECK(sol->prior_rows == 6);
    CHECK(sol->degrees_of_freedom == 40);
    CHECK(sol->weighted_rss_data.has_value());
    CHECK(sol->weighted_rss_prior.has_value());
    CHECK(sol->weighted_rss.has_value());
    if (sol->weighted_rss_data && sol->weighted_rss_prior && sol->weighted_rss) {
        const double omega = *sol->weighted_rss_data + *sol->weighted_rss_prior;
        const double xnorm = dnorm_of(ne, sol->x);
        const double tol = 16.0 * Bcert * (static_cast<double>(ref.rss_total) + xnorm * xnorm);
        SAY("401c Omega_data %.12g  Omega_prior %.12g  total %.12g  reference %.12g", *sol->weighted_rss_data, *sol->weighted_rss_prior, omega, static_cast<double>(ref.rss_total));
        CHECK(std::fabs(omega - static_cast<double>(ref.rss_total)) <= tol);
        CHECK(*sol->weighted_rss == omega);
    }
    CHECK(sol->provenance.find("priors=2;prior_rows=6;") != std::string::npos);

    // (b) an exact case: n = 3, integer data, sigma powers of two; solved in exact integer arithmetic (Cramer), the weights multiplied by 16
    {
        const long long a[5][3] = {{1, 0, 2}, {0, 1, 1}, {3, 1, 0}, {1, 2, 1}, {2, 1, 3}};
        const double sg[5] = {1.0, 2.0, 1.0, 4.0, 2.0};
        const long long w[5] = {16, 4, 16, 1, 4};                                    // 16 / sigma^2
        const long long rr[5] = {3, 1, 4, 1, 5};
        long long N16[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        long long b16[3] = {0, 0, 0};
        Data e;
        e.n = 3;
        e.m = 5;
        for (std::size_t k = 0; k < 5; ++k) {
            for (std::size_t i = 0; i < 3; ++i) {
                e.A.push_back(static_cast<double>(a[k][i]));
                b16[i] += w[k] * a[k][i] * rr[k];
                for (std::size_t j = 0; j < 3; ++j) N16[i][j] += w[k] * a[k][i] * a[k][j];
            }
            e.r.push_back(static_cast<double>(rr[k]));
            e.sigma.push_back(sg[k]);
        }
        N16[0][0] += 4;                                                              // the prior on parameter 0: sigma 2, centre 3: 16 / 4 = 4;  b += 4 * 3
        b16[0] += 4 * 3;
        N16[2][2] += 1;                                                              // on parameter 2: sigma 4, centre -1: 16 / 16 = 1;  b += 1 * (-1)
        b16[2] += -1;
        auto det3 = [](const long long m[3][3]) {
            return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        };
        const long long D = det3(N16);
        REQUIRE(D != 0);
        std::vector<ld> exact(3);
        for (std::size_t c = 0; c < 3; ++c) {
            long long M[3][3];
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) M[i][j] = (j == c) ? b16[i] : N16[i][j];
            exact[c] = static_cast<ld>(det3(M)) / static_cast<ld>(D);
        }
        auto ne3 = make_ne(names_of(3, "e"), e);
        auto blk = PriorBlock::isotropic({"e0", "e2"}, {3.0, -1.0}, {2.0, 4.0});
        REQUIRE(blk.has_value());
        REQUIRE(ne3.add_prior(*blk, std::vector<double>(3, 0.0)).has_value());
        const auto s3 = ne3.solve();
        REQUIRE(s3.has_value());
        const double phi3 = phi_of(ne3, s3->x, exact);
        SAY("401b exact case: phi %.3e  B %.3e  det(16 N) %.0f", phi3, s3->certified_relative_error, static_cast<double>(D));
        CHECK(phi3 <= s3->certified_relative_error);
    }

    // (d) no prior is step 2's solve, bit for bit: against the same formed system through system mode, and the provenance literal of EST-R-212
    {
        auto plain = make_ne(kP1Names, d);
        const auto sp = plain.solve();
        REQUIRE(sp.has_value());
        const auto fs = plain.formed_system();
        REQUIRE(fs.has_value());
        auto sysm = NormalEquations::from_normal_system(kP1Names, fs->matrix, fs->rhs, d.m);
        REQUIRE(sysm.has_value());
        const auto ss = sysm->solve();
        REQUIRE(ss.has_value());
        CHECK(sp->x == ss->x);
        CHECK(sp->scaled_matrix == ss->scaled_matrix);
        CHECK(sp->scaled_rhs == ss->scaled_rhs);
        CHECK(sp->scaled_solution == ss->scaled_solution);
        CHECK(sp->scaled_cofactor == ss->scaled_cofactor);
        CHECK(sp->cofactor == ss->cofactor);
        CHECK(sp->scale_exponents == ss->scale_exponents);
        CHECK(sp->prior_rows == 0);
        CHECK(sp->degrees_of_freedom == static_cast<std::ptrdiff_t>(d.m) - 6);
        CHECK(sp->weighted_rss.has_value());
        if (sp->weighted_rss && sp->weighted_rss_data) CHECK(*sp->weighted_rss == *sp->weighted_rss_data);
        std::string want = "estimation=batch-normal-equations;scaling=powers-of-two;parameters=6;free=6;observations=40;eliminated=none;exponents=";
        for (std::size_t i = 0; i < 6; ++i) want += (i ? "," : "") + std::to_string(sp->scale_exponents[i]);
        want += ";";
        CHECK(sp->provenance == want);
        CHECK(sp->provenance.find("priors=") == std::string::npos);
    }

    // controls (must fail by >= 10^3 B): the module is given a defective block; the reference keeps the correct one
    const auto control = [&](const PriorBlock& iso_c, const PriorBlock& rtn_c, const char* which) {
        auto nc = make_ne(kP1Names, d);
        REQUIRE(nc.add_prior(iso_c, x_lin).has_value());
        REQUIRE(nc.add_prior(rtn_c, x_lin).has_value());
        const auto sc = nc.solve();
        REQUIRE(sc.has_value());
        const double pc = phi_of(nc, sc->x, ref.x);
        std::printf("PRIOR %s control phi %.3e  B %.3e  ratio %.3e\n", which, pc, sc->certified_relative_error, pc / sc->certified_relative_error);
        CHECK(pc >= 1.0e3 * sc->certified_relative_error);   // NOT-A-UNIT-CROSSING: registered "10^3 B"
    };
    control(iso, p1_rtn(transposed(B)), "401-control-i-transposed-basis");
    control(p1_iso(-1.0), rtn, "401-control-ii-reversed-centres");
    control(p1_iso(1.0, 1.0 + 1.0e-6), rtn, "401-control-iii-sigma-1e-6");
}

// =====================================================================================================================================================================================
TEST_CASE("EST-A-402: the two limits of sigma and the exact marginal (Schur complement)", "[estimation][prior][a402]") {
    Rng g(kSeed + 2);
    Data d;
    d.n = 4;
    d.m = 30;
    d.A.resize(d.n * d.m);
    d.r.resize(d.m);
    d.sigma.resize(d.m);
    const double truth[4] = {0.2, -0.4, 0.7, 0.1};
    for (std::size_t k = 0; k < d.m; ++k) {
        d.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);
        double acc = 0.0;
        for (std::size_t j = 0; j < d.n; ++j) {
            d.A[k * d.n + j] = g.uni();
            acc += d.A[k * d.n + j] * truth[j];
        }
        d.r[k] = acc + 0.1 * d.sigma[k] * g.normal();
    }
    const auto names = names_of(4, "q");
    const std::vector<double> x_lin(4, 0.0);
    const double d0 = 2.0;

    auto nu = make_ne(names, d);
    const auto su = nu.solve();
    REQUIRE(su.has_value());
    const double c0u = su->x[0];
    const double Q00 = su->cofactor[0];
    REQUIRE(Q00 > 0.0);
    const double Bu = su->certified_relative_error;
    const double dnu = dnorm_of(nu, su->x);
    SAY("402setup Q00 %.6e  x0_unc %.9g  d0 %.3g  B_u %.3e", Q00, c0u, d0, Bu);

    auto ne_elim = make_ne(names, d);
    SolveOptions so;
    so.eliminate.push_back(Elimination{"q0", d0});
    const auto se = ne_elim.solve(so);
    REQUIRE(se.has_value());

    const double ks[10] = {1.0e-12, 1.0e-6, 1.0e-3, 1.0e-2, 1.0e-1, 1.0, 10.0, 1.0e2, 1.0e6, 1.0e12};
    std::vector<double> dev(10, 0.0);
    std::vector<double> x0s(10, 0.0);
    for (std::size_t ik = 0; ik < 10; ++ik) {
        const double k = ks[ik];
        const double sigma = k * std::sqrt(Q00);
        auto nc = make_ne(names, d);
        auto blk = PriorBlock::isotropic({"q0"}, {d0}, {sigma});
        REQUIRE(blk.has_value());
        REQUIRE(nc.add_prior(*blk, x_lin).has_value());
        const auto sc = nc.solve();
        REQUIRE(sc.has_value());                                                       // gated: B_c < 1/2 (no refusal) at every k
        const double Bc = sc->certified_relative_error;
        const ld f = static_cast<ld>(k) * static_cast<ld>(k) / (static_cast<ld>(k) * static_cast<ld>(k) + 1.0L);
        const ld pred = static_cast<ld>(d0) + (static_cast<ld>(c0u) - static_cast<ld>(d0)) * f;
        const auto fs = nc.formed_system();
        REQUIRE(fs.has_value());
        const double D00 = std::sqrt(fs->matrix[0]);
        const double err = D00 * std::fabs(sc->x[0] - static_cast<double>(pred));
        const double tol = (Bc + Bu) * (dnorm_of(nc, sc->x) + dnu);
        SAY("402identity k %.0e  x0 %.15g  predicted %.15g  scaled error %.3e  tolerance %.3e", k, sc->x[0], static_cast<double>(pred), err, tol);
        CHECK(err <= tol);
        dev[ik] = std::fabs(sc->x[0] - d0);
        x0s[ik] = sc->x[0];
        if (k == 1.0e12) {                                                             // sigma -> infinity: the unconstrained solution
            std::vector<ld> xu(su->x.begin(), su->x.end());
            const double ph = phi_of(nu, sc->x, xu);
            SAY("402inf phi to the unconstrained %.3e  B_c + B_u %.3e", ph, Bc + Bu);
            CHECK(ph <= Bc + Bu);
        }
        if (k <= 1.0e-6) {                                                             // sigma -> 0: the elimination of EST-R-209, on the free parameters
            std::vector<double> xc_free(sc->x.begin() + 1, sc->x.end());
            std::vector<ld> xe_free;
            for (std::size_t i = 1; i < 4; ++i) xe_free.push_back(static_cast<ld>(se->x[i]));
            const auto fe = ne_elim.formed_system();
            REQUIRE(fe.has_value());
            ld num = 0.0L, den = 0.0L;
            for (std::size_t i = 1; i < 4; ++i) {
                const ld Nii = static_cast<ld>(fe->matrix[i * 4 + i]);
                const ld e = static_cast<ld>(sc->x[i]) - xe_free[i - 1];
                num += Nii * e * e;
                den += Nii * xe_free[i - 1] * xe_free[i - 1];
            }
            const double ph = static_cast<double>(std::sqrt(num / den));
            SAY("402zero k %.0e  phi to the elimination %.3e  B_c + B_e %.3e   B_c %.3e  B_e %.3e", k, ph, Bc + se->certified_relative_error, Bc, se->certified_relative_error);
            CHECK(ph <= Bc + se->certified_relative_error);
            if (k == 1.0e-12) SAY("402zero-reported B_c / B_e = %.3f (predicted <= 10; reported, not gated)", Bc / se->certified_relative_error);
        }
    }
    // the O(sigma^2) law: the ratio of successive decades is about 100 (between 90 and 101)
    const double r1 = dev[4] / dev[3], r2 = dev[3] / dev[2];
    SAY("402decades dev(1e-1)/dev(1e-2) %.4f  dev(1e-2)/dev(1e-3) %.4f", r1, r2);
    CHECK((r1 >= 90.0 && r1 <= 101.0));
    CHECK((r2 >= 90.0 && r2 <= 101.0));
    // controls (must fail by >= 10^3 B at k = 1): the wrong sign of d0, and the wrong form sigma^2/(sigma^2 - Q) (singular at k = 1: a violation by definition) and at k = 2
    {
        const std::size_t i1 = 5;                                                      // k = 1
        const ld f1 = 0.5L;
        const ld wrong = -static_cast<ld>(d0) + (static_cast<ld>(c0u) + static_cast<ld>(d0)) * f1;
        auto nc = make_ne(names, d);
        auto blk = PriorBlock::isotropic({"q0"}, {d0}, {std::sqrt(Q00)});
        REQUIRE(blk.has_value());
        REQUIRE(nc.add_prior(*blk, x_lin).has_value());
        const auto sc = nc.solve();
        REQUIRE(sc.has_value());
        const auto fs = nc.formed_system();
        const double D00 = std::sqrt(fs->matrix[0]);
        const double viol = D00 * std::fabs(x0s[i1] - static_cast<double>(wrong)) / dnorm_of(nc, sc->x);
        SAY("402control wrong-sign prediction violated by %.3e (B_c %.3e, ratio %.3e)", viol, sc->certified_relative_error, viol / sc->certified_relative_error);
        CHECK(viol >= 1.0e3 * sc->certified_relative_error);   // NOT-A-UNIT-CROSSING: registered "10^3 B"
        const ld f2 = 4.0L / (4.0L - 1.0L);                                            // sigma^2 / (sigma^2 - Q) at k = 2
        const ld wrong2 = static_cast<ld>(d0) + (static_cast<ld>(c0u) - static_cast<ld>(d0)) * f2;
        auto nc2 = make_ne(names, d);
        auto blk2 = PriorBlock::isotropic({"q0"}, {d0}, {2.0 * std::sqrt(Q00)});
        REQUIRE(blk2.has_value());
        REQUIRE(nc2.add_prior(*blk2, x_lin).has_value());
        const auto sc2 = nc2.solve();
        REQUIRE(sc2.has_value());
        const auto fs2 = nc2.formed_system();
        const double viol2 = std::sqrt(fs2->matrix[0]) * std::fabs(sc2->x[0] - static_cast<double>(wrong2)) / dnorm_of(nc2, sc2->x);
        SAY("402control2 wrong-form prediction at k = 2 violated by %.3e (B_c %.3e)", viol2, sc2->certified_relative_error);
        CHECK(viol2 >= 1.0e3 * sc2->certified_relative_error);   // NOT-A-UNIT-CROSSING: registered "10^3 B"
    }
}

// =====================================================================================================================================================================================
namespace {

// a 3x3 orthogonal matrix from the generator (Gram-Schmidt of three normal vectors), optionally reflected
std::array<double, 9> seeded_orthogonal(Rng& g, bool reflect) {
    std::array<std::array<double, 3>, 3> v{};
    for (auto& row : v)
        for (double& x : row) x = g.normal();
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            double dot = 0.0;
            for (std::size_t k = 0; k < 3; ++k) dot += v[i][k] * v[j][k];
            for (std::size_t k = 0; k < 3; ++k) v[i][k] -= dot * v[j][k];
        }
        double nn = 0.0;
        for (std::size_t k = 0; k < 3; ++k) nn += v[i][k] * v[i][k];
        nn = std::sqrt(nn);
        for (std::size_t k = 0; k < 3; ++k) v[i][k] /= nn;
    }
    if (reflect)
        for (std::size_t k = 0; k < 3; ++k) v[2][k] = -v[2][k];
    return {v[0][0], v[0][1], v[0][2], v[1][0], v[1][1], v[1][2], v[2][0], v[2][1], v[2][2]};
}

struct VecProblem {
    Data d;                          // n = 4: "s", "x", "y", "z"
};
VecProblem make_vec(Rng& g) {
    VecProblem p;
    p.d.n = 4;
    p.d.m = 30;
    p.d.A.resize(p.d.n * p.d.m);
    p.d.r.resize(p.d.m);
    p.d.sigma.resize(p.d.m);
    const double truth[4] = {0.3, 1.0, -2.0, 0.5};
    for (std::size_t k = 0; k < p.d.m; ++k) {
        p.d.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);
        double acc = 0.0;
        for (std::size_t j = 0; j < 4; ++j) {
            p.d.A[k * 4 + j] = g.uni();
            acc += p.d.A[k * 4 + j] * truth[j];
        }
        p.d.r[k] = acc + 0.1 * p.d.sigma[k] * g.normal();
    }
    return p;
}

// the problem with the vector part (parameters 1..3) rotated by Q: columns A -> A Q^T on the block, the basis B -> B Q^T (or its transpose, the control), the centre -> Q x_A
struct Rotated {
    std::vector<double> x;           // the estimate, 4 values
    std::vector<double> cof;         // cofactor, 4 x 4
    double B = 0.0;
    std::vector<double> dvec;        // D = sqrt(N_ii)
};
Rotated solve_rotated(const Data& base, const std::array<double, 9>& Q, const std::array<double, 9>& B0, const std::array<double, 3>& sig, const std::array<double, 3>& centre,
                      bool use_rtn, bool transpose_basis) {
    Data d = base;
    for (std::size_t k = 0; k < d.m; ++k) {                                              // a' = Q a_v
        double v[3] = {base.A[k * 4 + 1], base.A[k * 4 + 2], base.A[k * 4 + 3]};
        for (std::size_t i = 0; i < 3; ++i) d.A[k * 4 + 1 + i] = Q[3 * i] * v[0] + Q[3 * i + 1] * v[1] + Q[3 * i + 2] * v[2];
    }
    std::array<double, 3> c2{};
    for (std::size_t i = 0; i < 3; ++i) c2[i] = Q[3 * i] * centre[0] + Q[3 * i + 1] * centre[1] + Q[3 * i + 2] * centre[2];
    auto ne = make_ne({"s", "x", "y", "z"}, d);
    auto iso_s = PriorBlock::isotropic({"s"}, {0.5}, {1.0});
    REQUIRE(iso_s.has_value());
    REQUIRE(ne.add_prior(*iso_s, std::vector<double>(4, 0.0)).has_value());
    std::array<double, 9> Bq{};                                                          // B' = B0 Q^T
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < 3; ++k) s += B0[3 * i + k] * Q[3 * j + k];
            Bq[3 * i + j] = s;
        }
    if (transpose_basis) Bq = transposed(Bq);
    if (use_rtn) {
        auto blk = PriorBlock::rtn({"x", "y", "z"}, c2, Bq, sig);
        REQUIRE(blk.has_value());
        REQUIRE(ne.add_prior(*blk, std::vector<double>(4, 0.0)).has_value());
    } else {
        auto blk = PriorBlock::isotropic({"x", "y", "z"}, {c2[0], c2[1], c2[2]}, {sig[0], sig[0], sig[0]});
        REQUIRE(blk.has_value());
        REQUIRE(ne.add_prior(*blk, std::vector<double>(4, 0.0)).has_value());
    }
    const auto sol = ne.solve();
    REQUIRE(sol.has_value());
    Rotated r;
    r.x = sol->x;
    r.cof = sol->cofactor;
    r.B = sol->certified_relative_error;
    const auto fs = ne.formed_system();
    for (std::size_t i = 0; i < 4; ++i) r.dvec.push_back(std::sqrt(fs->matrix[i * 4 + i]));
    return r;
}

}  // namespace

TEST_CASE("EST-A-403: frame invariance of the quadratic form, orthogonal invariance, the transposition control and the bits (a, b, c)", "[estimation][prior][a403]") {
    Rng g(kSeed + 3);
    const std::array<double, 9> B = fixture_basis();

    // (a) the quadratic form through with_priors at x = x_A + delta with a data model of one zero row: F_tot = 1/2 delta^T Lambda delta, against the same form computed in RTN
    {
        const std::array<double, 3> xa{1.5, -2.0, 0.7};
        const std::array<double, 3> sg{0.2, 200.0, 0.2};
        auto blk = PriorBlock::rtn({"x", "y", "z"}, xa, B, sg);
        REQUIRE(blk.has_value());
        const LmModel zero = [](std::span<const double>, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            LmEvaluation e;
            e.residual = {0.0};
            if (jac) e.jacobian = {0.0, 0.0, 0.0};
            return e;
        };
        auto wm = with_priors(zero, {"x", "y", "z"}, {*blk});
        REQUIRE(wm.has_value());
        double worst = 0.0;
        for (int t = 0; t < 10; ++t) {
            double dir[3] = {g.normal(), g.normal(), g.normal()};
            const double dn = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
            const double mag = std::pow(10.0, 2.0 * g.uni());                         // 10^-2 .. 10^2
            double delta[3];
            for (int i = 0; i < 3; ++i) delta[i] = mag * dir[i] / dn;
            const double x[3] = {xa[0] + delta[0], xa[1] + delta[1], xa[2] + delta[2]};
            const auto ev = (*wm)(std::span<const double>(x, 3), true);
            REQUIRE(ev.has_value());
            double F = 0.0;
            for (const double r : ev->residual) F += r * r;
            F *= 0.5;
            ld Fr = 0.0L;                                                              // in RTN: 1/2 sum_k ((B delta)_k / sigma_k)^2, with the DEFINITION's delta = x - x_A
            for (std::size_t k = 0; k < 3; ++k) {
                ld bd = 0.0L;
                for (std::size_t j = 0; j < 3; ++j) bd += static_cast<ld>(B[3 * k + j]) * (static_cast<ld>(x[j]) - static_cast<ld>(xa[j]));
                const ld q = bd / static_cast<ld>(sg[k]);
                Fr += q * q;
            }
            Fr *= 0.5L;
            const double rel = static_cast<double>(std::fabs(static_cast<ld>(F) - Fr) / Fr);
            worst = std::max(worst, rel);
            CHECK(rel <= 1.0e-13);
        }
        SAY("403a ten deltas: worst relative difference %.3e (bound 1e-13)", worst);
    }

    // (b) orthogonal invariance: the vector block rotated (and reflected) by a seeded Q; the estimate rotates, the cofactor rotates; the transposed basis breaks it
    {
        const VecProblem prob = make_vec(g);
        const std::array<double, 9> I3{1, 0, 0, 0, 1, 0, 0, 0, 1};
        const std::array<double, 3> sig{0.2, 200.0, 0.2};
        const std::array<double, 3> centre{1.0, -1.0, 0.5};
        for (int use_rtn = 1; use_rtn >= 0; --use_rtn) {
            for (int refl = 0; refl < 2; ++refl) {
                const std::array<double, 9> Q = seeded_orthogonal(g, refl != 0);
                const std::array<double, 3> sg_use = use_rtn ? sig : std::array<double, 3>{0.5, 0.5, 0.5};
                const Rotated base = solve_rotated(prob.d, I3, B, sg_use, centre, use_rtn != 0, false);
                const Rotated rot = solve_rotated(prob.d, Q, B, sg_use, centre, use_rtn != 0, false);
                // back-rotate the vector part: x_back = Q^T x'
                std::vector<double> xb = rot.x;
                for (std::size_t i = 0; i < 3; ++i) xb[1 + i] = Q[i] * rot.x[1] + Q[3 + i] * rot.x[2] + Q[6 + i] * rot.x[3];
                double dmax = 0.0, dmin = std::numeric_limits<double>::infinity();
                for (std::size_t i = 0; i < 4; ++i) {
                    dmax = std::max({dmax, base.dvec[i], rot.dvec[i]});
                    dmin = std::min({dmin, base.dvec[i], rot.dvec[i]});
                }
                double en = 0.0, xn = 0.0;
                for (std::size_t i = 0; i < 4; ++i) {
                    en += (xb[i] - base.x[i]) * (xb[i] - base.x[i]);
                    xn += base.x[i] * base.x[i];
                }
                const double rel = std::sqrt(en / xn);
                const double tol = (base.B + rot.B) * (dmax / dmin);                  // the sum of the two certified errors, in the Euclidean norm (D ratio)
                SAY("403b rtn=%.0f reflect=%.0f  relative difference %.3e  tolerance %.3e", static_cast<double>(use_rtn), static_cast<double>(refl), rel, tol);
                CHECK(rel <= tol);
                // the cofactor: Q' = R Q_post R^T with R = blockdiag(1, Q)
                double ce = 0.0, cn = 0.0;
                for (std::size_t i = 0; i < 4; ++i)
                    for (std::size_t j = 0; j < 4; ++j) {
                        double s = 0.0;
                        for (std::size_t a = 0; a < 4; ++a)
                            for (std::size_t b = 0; b < 4; ++b) {
                                const double ra = (i == 0) ? (a == 0 ? 1.0 : 0.0) : (a == 0 ? 0.0 : Q[3 * (i - 1) + (a - 1)]);
                                const double rb = (j == 0) ? (b == 0 ? 1.0 : 0.0) : (b == 0 ? 0.0 : Q[3 * (j - 1) + (b - 1)]);
                                s += ra * base.cof[a * 4 + b] * rb;
                            }
                        ce += (s - rot.cof[i * 4 + j]) * (s - rot.cof[i * 4 + j]);
                        cn += s * s;
                    }
                CHECK(std::sqrt(ce / cn) <= tol * 4.0);                                 // test-level: the cofactor's error is a few times the solution's (executor-classified)
                if (use_rtn) {                                                           // the control: the transposed basis breaks the equality by >= 10^3 (B + B')
                    const Rotated bad = solve_rotated(prob.d, Q, B, sg_use, centre, true, true);
                    std::vector<double> xbad = bad.x;
                    for (std::size_t i = 0; i < 3; ++i) xbad[1 + i] = Q[i] * bad.x[1] + Q[3 + i] * bad.x[2] + Q[6 + i] * bad.x[3];
                    double e2 = 0.0;
                    for (std::size_t i = 0; i < 4; ++i) e2 += (xbad[i] - base.x[i]) * (xbad[i] - base.x[i]);
                    const double relbad = std::sqrt(e2 / xn);
                    SAY("403b-control transposed basis: relative difference %.3e (must be >= 1e3 * %.3e)", relbad, base.B + bad.B);
                    CHECK(relbad >= 1.0e3 * (base.B + bad.B) * (dmax / dmin));   // NOT-A-UNIT-CROSSING: registered "10^3 B"
                }
            }
        }
    }

    // (c) the bits: powers of two on the columns and the prior's sigma and centre (EST-R-405); the exponents move by p, everything else is equal or scales exactly
    const auto strip_exponents = [](std::string s) {
        const auto a = s.find("exponents=");
        const auto b = s.find(';', a);
        s.erase(a, b - a + 1);
        return s;
    };
    const auto compare = [&](const Solution& a, const Solution& b, const std::vector<int>& p, const char* tag) {
        const std::size_t n = a.x.size();
        CHECK(a.scaled_matrix == b.scaled_matrix);
        CHECK(a.scaled_rhs == b.scaled_rhs);
        CHECK(a.scaled_solution == b.scaled_solution);
        CHECK(a.scaled_cofactor == b.scaled_cofactor);
        CHECK(a.condition.condition_estimate == b.condition.condition_estimate);
        CHECK(a.condition.condition_upper == b.condition.condition_upper);
        CHECK(a.condition.parameter_conditions == b.condition.parameter_conditions);
        CHECK(a.condition.weakest_direction == b.condition.weakest_direction);
        CHECK(a.certified_relative_error == b.certified_relative_error);
        CHECK(a.weighted_rss == b.weighted_rss);
        CHECK(a.weighted_rss_data == b.weighted_rss_data);
        CHECK(a.weighted_rss_prior == b.weighted_rss_prior);
        CHECK(a.degrees_of_freedom == b.degrees_of_freedom);
        CHECK(a.prior_rows == b.prior_rows);
        CHECK(strip_exponents(a.provenance) == strip_exponents(b.provenance));
        bool exps = true, xs = true, cs = true;
        for (std::size_t i = 0; i < n; ++i) {
            exps = exps && (b.scale_exponents[i] == a.scale_exponents[i] + p[i]);
            xs = xs && (b.x[i] == std::ldexp(a.x[i], -p[i]));
            for (std::size_t j = 0; j < n; ++j) cs = cs && (b.cofactor[i * n + j] == std::ldexp(a.cofactor[i * n + j], -p[i] - p[j]));
        }
        CHECK(exps);
        CHECK(xs);
        CHECK(cs);
        std::printf("PRIOR %s bit-identity checks done (exponents moved by p, x and the cofactor scaled exactly)\n", tag);
    };
    {   // three isotropic parameters, p = (3, -2, 5), a fourth parameter unconstrained and unscaled
        Data d0;
        d0.n = 4;
        d0.m = 20;
        d0.A.resize(d0.n * d0.m);
        d0.r.resize(d0.m);
        d0.sigma.resize(d0.m);
        for (std::size_t k = 0; k < d0.m; ++k) {
            d0.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);
            double acc = 0.0;
            for (std::size_t j = 0; j < 4; ++j) {
                d0.A[k * 4 + j] = g.uni();
                acc += d0.A[k * 4 + j] * (0.3 + 0.2 * static_cast<double>(j));
            }
            d0.r[k] = acc + 0.1 * d0.sigma[k] * g.normal();
        }
        const std::vector<int> p{3, -2, 5, 0};
        const auto run = [&](const std::vector<int>& sh) {
            Data d = d0;
            for (std::size_t k = 0; k < d.m; ++k)
                for (std::size_t j = 0; j < 4; ++j) d.A[k * 4 + j] = std::ldexp(d0.A[k * 4 + j], sh[j]);
            auto ne = make_ne({"a", "b", "c", "d"}, d);
            const double sg0[3] = {0.5, 2.0, 8.0}, ce0[3] = {1.0, -1.0, 0.5};
            std::vector<double> sg, ce;
            for (std::size_t j = 0; j < 3; ++j) {
                sg.push_back(std::ldexp(sg0[j], -sh[j]));
                ce.push_back(std::ldexp(ce0[j], -sh[j]));
            }
            auto blk = PriorBlock::isotropic({"a", "b", "c"}, ce, sg);
            REQUIRE(blk.has_value());
            REQUIRE(ne.add_prior(*blk, std::vector<double>(4, 0.0)).has_value());
            auto sol = ne.solve();
            REQUIRE(sol.has_value());
            return *sol;
        };
        const Solution s0 = run({0, 0, 0, 0}), s1 = run(p);
        compare(s0, s1, p, "403c-isotropic");
    }
    for (const int pc : {-7, 4, 11}) {   // an RTN block, one common power on all three components
        Data d0;
        d0.n = 4;
        d0.m = 20;
        d0.A.resize(d0.n * d0.m);
        d0.r.resize(d0.m);
        d0.sigma.resize(d0.m);
        for (std::size_t k = 0; k < d0.m; ++k) {
            d0.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);
            double acc = 0.0;
            for (std::size_t j = 0; j < 4; ++j) {
                d0.A[k * 4 + j] = g.uni();
                acc += d0.A[k * 4 + j] * (0.3 + 0.2 * static_cast<double>(j));
            }
            d0.r[k] = acc + 0.1 * d0.sigma[k] * g.normal();
        }
        const std::vector<int> p{pc, pc, pc, 0};
        const auto run = [&](const std::vector<int>& sh) {
            Data d = d0;
            for (std::size_t k = 0; k < d.m; ++k)
                for (std::size_t j = 0; j < 4; ++j) d.A[k * 4 + j] = std::ldexp(d0.A[k * 4 + j], sh[j]);
            auto ne = make_ne({"x", "y", "z", "w"}, d);
            const std::array<double, 3> sg0{0.2, 200.0, 0.2};
            std::array<double, 3> sg{}, ce{};
            const double ce0[3] = {1.0, -1.0, 0.5};
            for (std::size_t j = 0; j < 3; ++j) {
                sg[j] = std::ldexp(sg0[j], -sh[0]);
                ce[j] = std::ldexp(ce0[j], -sh[0]);
            }
            auto blk = PriorBlock::rtn({"x", "y", "z"}, ce, B, sg);
            REQUIRE(blk.has_value());
            REQUIRE(ne.add_prior(*blk, std::vector<double>(4, 0.0)).has_value());
            auto sol = ne.solve();
            REQUIRE(sol.has_value());
            return *sol;
        };
        const Solution s0 = run({0, 0, 0, 0}), s1 = run(p);
        compare(s0, s1, p, "403c-rtn");
        if (pc == 4) {   // reported, not gated: one component of an RTN block scaled by 2 alone changes the solution (the boundary of EST-R-405)
            Data d = d0;
            for (std::size_t k = 0; k < d.m; ++k) d.A[k * 4] = std::ldexp(d0.A[k * 4], 1);
            auto ne = make_ne({"x", "y", "z", "w"}, d);
            const std::array<double, 3> sg{0.2, 200.0, 0.2};
            auto blk = PriorBlock::rtn({"x", "y", "z"}, {1.0, -1.0, 0.5}, B, sg);
            REQUIRE(blk.has_value());
            REQUIRE(ne.add_prior(*blk, std::vector<double>(4, 0.0)).has_value());
            const auto sol = ne.solve();
            REQUIRE(sol.has_value());
            double dd = 0.0, nn = 0.0;
            for (std::size_t i = 0; i < 4; ++i) {
                const double back = (i == 0) ? std::ldexp(sol->x[0], 1) : sol->x[i];
                dd += (back - s0.x[i]) * (back - s0.x[i]);
                nn += s0.x[i] * s0.x[i];
            }
            SAY("403c-boundary-reported one component scaled by 2 alone: relative change of the solution %.3e (certified B %.3e)", std::sqrt(dd / nn), s0.certified_relative_error);
        }
    }
}

// =====================================================================================================================================================================================
TEST_CASE("EST-A-404: the posterior covariance, Q_post^-1 - N = Lambda, and the Loewner order", "[estimation][prior][a404]") {
    Rng g(kSeed + 4);
    Data d;
    d.n = 5;
    d.m = 30;
    d.A.resize(d.n * d.m);
    d.r.resize(d.m);
    d.sigma.resize(d.m);
    const double truth[5] = {0.1, -0.2, 0.3, 0.4, -0.5};
    for (std::size_t k = 0; k < d.m; ++k) {
        d.sigma[k] = 0.5 + 0.75 * (g.uni() + 1.0);
        double acc = 0.0;
        for (std::size_t j = 0; j < d.n; ++j) {
            d.A[k * d.n + j] = 10.0 * g.uni();
            acc += d.A[k * d.n + j] * truth[j];
        }
        d.r[k] = acc + 0.1 * d.sigma[k] * g.normal();
    }
    const auto names = names_of(5, "u");
    const std::vector<double> x_lin(5, 0.0);
    const std::array<double, 9> B = fixture_basis();
    auto iso = PriorBlock::isotropic({"u0", "u1"}, {0.5, -0.5}, {1.0, 0.5});
    auto rtn = PriorBlock::rtn({"u2", "u3", "u4"}, {0.2, 0.1, -0.1}, B, {0.2, 200.0, 0.2});
    REQUIRE(iso.has_value());
    REQUIRE(rtn.has_value());

    auto nd = make_ne(names, d);
    const auto sd = nd.solve();
    REQUIRE(sd.has_value());
    auto np = make_ne(names, d);
    REQUIRE(np.add_prior(*iso, x_lin).has_value());
    REQUIRE(np.add_prior(*rtn, x_lin).has_value());
    const auto sp = np.solve();
    REQUIRE(sp.has_value());
    const double Bc = sp->certified_relative_error, Bu = sd->certified_relative_error;
    const std::size_t n = 5;

    // Lambda from the test's own formula, long double
    Mat Lam(n * n, 0.0L), LamInv(n * n, 0.0L);
    for (std::size_t i = 0; i < 2; ++i) {
        const ld s = static_cast<ld>(iso->sigma[i]);
        Lam[i * n + i] = 1.0L / (s * s);
        LamInv[i * n + i] = s * s;
    }
    for (std::size_t a = 0; a < 3; ++a)
        for (std::size_t b = 0; b < 3; ++b) {
            ld s = 0.0L, si = 0.0L;
            for (std::size_t k = 0; k < 3; ++k) {
                const ld sg = static_cast<ld>(rtn->sigma[k]);
                s += static_cast<ld>(B[3 * k + a]) * static_cast<ld>(B[3 * k + b]) / (sg * sg);
                si += static_cast<ld>(B[3 * k + a]) * static_cast<ld>(B[3 * k + b]) * sg * sg;
            }
            Lam[(2 + a) * n + 2 + b] = s;
            LamInv[(2 + a) * n + 2 + b] = si;
        }
    const auto fd = nd.formed_system();
    REQUIRE(fd.has_value());
    Mat Ntot(n * n);
    for (std::size_t i = 0; i < n * n; ++i) Ntot[i] = static_cast<ld>(fd->matrix[i]) + Lam[i];

    // (a) Q_post^-1 = N + Lambda: the inverse of the reported cofactor against N + Lambda, in the scaled (D) norm of N + Lambda, within 4 B
    {
        Mat Qp(n * n);
        for (std::size_t i = 0; i < n * n; ++i) Qp[i] = static_cast<ld>(sp->cofactor[i]);
        const Mat inv = inverse(Qp, n);
        ld num = 0.0L, den = 0.0L;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) {
                const ld dd = std::sqrt(Ntot[i * n + i] * Ntot[j * n + j]);
                const ld e = (inv[i * n + j] - Ntot[i * n + j]) / dd;
                num += e * e;
                den += (Ntot[i * n + j] / dd) * (Ntot[i * n + j] / dd);
            }
        const double rel = static_cast<double>(std::sqrt(num / den));
        SAY("404a |Q_post^-1 - (N + Lambda)| (D-scaled, Frobenius, relative) %.3e  tolerance 4 B_c = %.3e", rel, 4.0 * Bc);
        CHECK(rel <= 4.0 * Bc);
    }
    // the directions: 10^4 seeded unit vectors in the variables scaled by D_u = diag(sqrt(N_ii)) of the data system, the five axes, the weakest direction
    const auto form = [&](const std::vector<double>& Q, const std::vector<ld>& v) {                // v^T (D Q D) v
        ld s = 0.0L;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) s += v[i] * std::sqrt(static_cast<ld>(fd->matrix[i * n + i])) * static_cast<ld>(Q[i * n + j]) * std::sqrt(static_cast<ld>(fd->matrix[j * n + j])) * v[j];
        return s;
    };
    const auto formM = [&](const Mat& Q, const std::vector<ld>& v) {
        ld s = 0.0L;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) s += v[i] * std::sqrt(static_cast<ld>(fd->matrix[i * n + i])) * Q[i * n + j] * std::sqrt(static_cast<ld>(fd->matrix[j * n + j])) * v[j];
        return s;
    };
    std::vector<std::vector<ld>> dirs;
    for (int t = 0; t < 10000; ++t) {   // NOT-A-UNIT-CROSSING: the registered count of directions, 10^4
        std::vector<ld> v(n);
        ld nn = 0.0L;
        for (auto& x : v) {
            x = static_cast<ld>(g.normal());
            nn += x * x;
        }
        nn = std::sqrt(nn);
        for (auto& x : v) x /= nn;
        dirs.push_back(v);
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<ld> v(n, 0.0L);
        v[i] = 1.0L;
        dirs.push_back(v);
    }
    {
        std::vector<ld> v(sd->condition.weakest_direction.begin(), sd->condition.weakest_direction.end());   // the weakest direction of the data system (scaled variables of ITS exponents; a unit vector, used as one more direction)
        dirs.push_back(v);
    }
    const double tolB = Bc + Bu;
    // (b) Q_post <= Q_data
    {
        ld worst = 1.0e300L;
        for (const auto& v : dirs) {
            const ld qd = form(sd->cofactor, v), qp = form(sp->cofactor, v);
            const ld margin = (qd - qp) / qd;                                             // must be >= -B
            worst = std::min(worst, margin);
            CHECK(margin >= -static_cast<ld>(tolB));
        }
        ld trd = 0.0L, trp = 0.0L;
        for (std::size_t i = 0; i < n; ++i) {
            trd += static_cast<ld>(sd->cofactor[i * n + i]);
            trp += static_cast<ld>(sp->cofactor[i * n + i]);
        }
        SAY("404b smallest relative margin (Q_data - Q_post)/Q_data over %.0f directions: %.3e (bound >= -%.3e);  tr Q_data - tr Q_post = %.6e", static_cast<double>(dirs.size()), static_cast<double>(worst), tolB,
            static_cast<double>(trd - trp));
        CHECK(trd - trp > 0.0L);
    }
    // (c) Q_post <= Lambda^-1
    {
        ld worst = 1.0e300L;
        for (const auto& v : dirs) {
            const ld li = formM(LamInv, v), qp = form(sp->cofactor, v);
            const ld margin = (li - qp) / li;
            worst = std::min(worst, margin);
            CHECK(margin >= -static_cast<ld>(Bc));
        }
        SAY("404c smallest relative margin (Lambda^-1 - Q_post)/Lambda^-1: %.3e (bound >= -%.3e)", static_cast<double>(worst), Bc);
    }
    // control (must fail): N - Lambda, a NEGATIVE information, built here (not through the API): its inverse is above Q_data and (b) must be violated by >= 10^3 B on some direction
    {
        Mat Nm(n * n);
        for (std::size_t i = 0; i < n * n; ++i) Nm[i] = static_cast<ld>(fd->matrix[i]) - Lam[i];
        const Mat Qm = inverse(Nm, n);
        ld worst = 1.0e300L;
        for (const auto& v : dirs) {
            const ld qd = form(sd->cofactor, v), qm = formM(Qm, v);
            worst = std::min(worst, (qd - qm) / qd);
        }
        SAY("404control N - Lambda: smallest margin %.3e (a violation must reach <= -1e3 * %.3e)", static_cast<double>(worst), tolB);
        CHECK(worst <= -1.0e3L * static_cast<ld>(tolB));   // NOT-A-UNIT-CROSSING: registered "10^3 B"
    }
}

// =====================================================================================================================================================================================
namespace {

// the decay model of EST-A-405: y = a exp(-b t), t = 0, 0.05, ..., 0.25, sigma = 0.01
constexpr std::size_t kM405 = 6;
const double kT405[kM405] = {0.0, 0.05, 0.10, 0.15, 0.20, 0.25};
const double kOff405[kM405] = {0.004, -0.003, 0.002, -0.004, 0.003, -0.002};
constexpr double kSig405 = 0.01;

double y405(std::size_t k) { return 2.0 * std::exp(-0.5 * kT405[k]) + kOff405[k]; }

LmModel decay405() {
    return [](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(kM405);
        if (jac) e.jacobian.assign(kM405 * 2, 0.0);
        for (std::size_t k = 0; k < kM405; ++k) {
            const double ex = std::exp(-p[1] * kT405[k]);
            e.residual[k] = (y405(k) - p[0] * ex) / kSig405;
            if (jac) {
                e.jacobian[2 * k] = ex / kSig405;
                e.jacobian[2 * k + 1] = -p[0] * kT405[k] * ex / kSig405;
            }
        }
        return e;
    };
}

// the independent F_tot = 1/2 sum r^2 + 1/2 sum ((x_i - xA_i)/sigma_i)^2, in long double
ld ftot405(const double* x) {
    ld s = 0.0L;
    for (std::size_t k = 0; k < kM405; ++k) {
        const ld r = (static_cast<ld>(y405(k)) - static_cast<ld>(x[0]) * std::exp(-static_cast<ld>(x[1]) * static_cast<ld>(kT405[k]))) / static_cast<ld>(kSig405);
        s += r * r;
    }
    const ld a = (static_cast<ld>(x[0]) - 2.0L) / 0.5L, b = (static_cast<ld>(x[1]) - 0.5L) / 0.25L;
    s += a * a + b * b;
    return 0.5L * s;
}

}  // namespace

TEST_CASE("EST-A-405: the damped method sees the whole objective (the prior as rows), with controls that fail", "[estimation][prior][a405]") {
    const std::vector<std::string> names = {"a", "b"};
    const double x0[2] = {20.0, 4.0};
    auto blk = PriorBlock::isotropic({"a", "b"}, {2.0, 0.5}, {0.5, 0.25});
    REQUIRE(blk.has_value());

    // the registered premise (reported, not gated): without the prior, LM from this start does not end converged_gradient
    {
        const LmResult free_run = levenberg_marquardt(names, x0, decay405());
        std::printf("PRIOR 405premise (reported, not gated): without the prior, status %s after %zu passes (%zu accepted, %zu rejected); premise 'not converged_gradient' %s\n", to_string(free_run.status),
                    free_run.iterations, free_run.accepted_steps, free_run.rejected_steps, free_run.status != LmStatus::converged_gradient ? "HELD" : "FAILED (a finding)");
    }

    auto wm = with_priors(decay405(), names, {*blk});
    REQUIRE(wm.has_value());
    const LmResult r = levenberg_marquardt(names, x0, *wm);
    std::printf("PRIOR 405run status %s  iterations %zu  accepted %zu  rejected %zu  objective %.15g  x = (%.12g, %.12g)  cosine %.3e\n", to_string(r.status), r.iterations, r.accepted_steps,
                r.rejected_steps, r.objective, r.x[0], r.x[1], r.cosine);
    CHECK((r.status == LmStatus::converged_gradient || r.status == LmStatus::converged_step));
    // the final objective equals the independent F_tot
    {
        const ld F = ftot405(r.x.data());
        const double rel = static_cast<double>(std::fabs(static_cast<ld>(r.objective) - F) / F);
        std::printf("PRIOR 405objective LM %.17g  independent %.17Lg  relative %.3e\n", r.objective, F, rel);
        CHECK(rel <= 1.0e-12);   // NOT-A-UNIT-CROSSING: registered "10^-12"
    }
    // in every pass the recorded objective is the independent F_tot at that pass's iterate, and every accepted pass lowers it
    {
        std::vector<double> cur{x0[0], x0[1]};
        std::size_t acc = 0;
        double worst = 0.0;
        bool lowered = true;
        for (std::size_t k = 0; k < r.history.size(); ++k) {
            const ld F = ftot405(cur.data());
            const double rel = static_cast<double>(std::fabs(static_cast<ld>(r.history[k].objective) - F) / F);
            worst = std::max(worst, rel);
            CHECK(rel <= 1.0e-12);   // NOT-A-UNIT-CROSSING: registered "10^-12"
            if (r.history[k].accepted) {
                REQUIRE(acc < r.accepted_iterates.size());
                const auto& nx = r.accepted_iterates[acc++];
                lowered = lowered && (ftot405(nx.data()) < F);
                cur = nx;
            }
        }
        SAY("405history worst per-pass relative difference to the independent F_tot %.3e over %.0f passes", worst, static_cast<double>(r.history.size()));
        CHECK(lowered);
    }
    // the first-order condition of the WHOLE objective: the independent central-difference gradient, cosine <= 1e-6 (max_i |g_i| / (sqrt(N_ii) |r|), the module's own definition)
    {
        const double h = 1.0e-6;   // NOT-A-UNIT-CROSSING: a finite-difference step, dimensionless here
        const auto ev = (*wm)(std::span<const double>(r.x.data(), 2), true);
        REQUIRE(ev.has_value());
        double rn2 = 0.0;
        for (const double v : ev->residual) rn2 += v * v;
        double cmax = 0.0;
        for (std::size_t i = 0; i < 2; ++i) {
            double xp[2] = {r.x[0], r.x[1]}, xm[2] = {r.x[0], r.x[1]};
            const double hh = h * std::max(std::fabs(r.x[i]), 1.0);
            xp[i] += hh;
            xm[i] -= hh;
            const double g = static_cast<double>((ftot405(xp) - ftot405(xm)) / (2.0L * static_cast<ld>(hh)));
            double Nii = 0.0;
            for (std::size_t k = 0; k < ev->residual.size(); ++k) Nii += ev->jacobian[k * 2 + i] * ev->jacobian[k * 2 + i];
            cmax = std::max(cmax, std::fabs(g) / (std::sqrt(Nii) * std::sqrt(rn2)));
        }
        SAY("405cosine independent finite-difference gradient cosine at the final iterate: %.3e (bound 1e-6)", cmax);
        CHECK(cmax <= 1.0e-6);   // NOT-A-UNIT-CROSSING: registered "10^-6"
    }
    // the final system's cofactor is (N + Lambda)^-1 at that point
    {
        REQUIRE(r.final_system.has_value());
        const auto ev = (*wm)(std::span<const double>(r.x.data(), 2), true);
        Mat Nt(4, 0.0L);
        for (std::size_t k = 0; k < ev->residual.size(); ++k)
            for (std::size_t i = 0; i < 2; ++i)
                for (std::size_t j = 0; j < 2; ++j) Nt[i * 2 + j] += static_cast<ld>(ev->jacobian[k * 2 + i]) * static_cast<ld>(ev->jacobian[k * 2 + j]);
        const Mat Qi = inverse(Nt, 2);
        double worst = 0.0;
        for (std::size_t i = 0; i < 2; ++i)
            for (std::size_t j = 0; j < 2; ++j) {
                const double ref = static_cast<double>(Qi[i * 2 + j]);
                const double den = std::sqrt(static_cast<double>(Qi[i * 2 + i]) * static_cast<double>(Qi[j * 2 + j]));
                worst = std::max(worst, std::fabs(r.final_system->cofactor[i * 2 + j] - ref) / den);
            }
        SAY("405cofactor final_system cofactor against (N + Lambda)^-1: worst entry difference relative to sqrt(Q_ii Q_jj) %.3e (test-level bound 1e-9)", worst);
        CHECK(worst <= 1.0e-9);   // NOT-A-UNIT-CROSSING: a test-level tolerance, executor-classified
    }
    // control (must fail): the same rows with LmEvaluation::objective set to the DATA-ONLY 1/2 sum r^2 (the wrong convention EST-F-404 refuses): the recorded objective at the start differs from F_tot by the prior's half
    {
        const LmModel inner = *wm;
        const LmModel wrong = [inner](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            auto e = inner(p, jac);
            if (!e) return e;
            double s = 0.0;
            for (std::size_t k = 0; k < kM405; ++k) s += e->residual[k] * e->residual[k];
            e->objective = 0.5 * s;
            return e;
        };
        LmOptions o;
        o.max_iterations = 1;
        const LmResult rw = levenberg_marquardt(names, x0, wrong, o);
        REQUIRE(!rw.history.empty());
        const ld F = ftot405(x0);
        const double rel = static_cast<double>(std::fabs(static_cast<ld>(rw.history[0].objective) - F) / F);
        SAY("405control data-only objective at the start: recorded %.9g, independent F_tot %.9g, relative difference %.3e (must be >= 1e-3)", rw.history[0].objective, static_cast<double>(F), rel);
        CHECK(rel >= 1.0e-3);   // NOT-A-UNIT-CROSSING: registered "10^-3" relative
    }
    // and with_priors over a model that carries an objective returns EST-F-404
    {
        const LmModel carries = [](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            auto e = decay405()(p, jac);
            e->objective = 1.0;
            return e;
        };
        auto wc = with_priors(carries, names, {*blk});
        REQUIRE(wc.has_value());
        const auto ev = (*wc)(std::span<const double>(x0, 2), true);
        REQUIRE(!ev.has_value());
        CHECK(std::string(ev.error().id) == "EST-F-404");
        const LmResult rc = levenberg_marquardt(names, x0, *wc);
        CHECK(rc.status == LmStatus::refused);
    }
}

// =====================================================================================================================================================================================
namespace {


}  // namespace

TEST_CASE("EST-A-406: the refusals EST-F-401 .. 406, non-bypassable, and observability", "[estimation][prior][a406]") {
    Rng g(kSeed + 6);
    const std::vector<std::string> names = {"p0", "p1", "p2", "p3"};
    Data d;
    d.n = 4;
    d.m = 12;
    d.A.resize(d.n * d.m);
    d.r.resize(d.m);
    d.sigma.assign(d.m, 1.0);
    for (std::size_t k = 0; k < d.m; ++k) {
        for (std::size_t j = 0; j < 4; ++j) d.A[k * 4 + j] = g.uni();
        d.r[k] = g.uni();
    }
    const std::vector<double> x_lin(4, 0.0);
    const std::array<double, 9> B = fixture_basis();

    // a refused call leaves the object unchanged and the same call without the defect succeeds
    const auto refused_add = [&](const PriorBlock& bad, const PriorBlock& good, const char* id, const char* must_name) {
        auto ne = make_ne(names, d);
        const auto before = ne.formed_system();
        REQUIRE(before.has_value());
        const auto r = ne.add_prior(bad, x_lin);
        REQUIRE(!r.has_value());
        CHECK(std::string(r.error().id) == id);
        CHECK(r.error().message.find(must_name) != std::string::npos);
        const auto after = ne.formed_system();
        REQUIRE(after.has_value());
        CHECK(before->matrix == after->matrix);
        CHECK(before->rhs == after->rhs);
        CHECK(ne.add_prior(good, x_lin).has_value());
    };
    const PriorBlock good_iso = *PriorBlock::isotropic({"p0", "p1"}, {0.1, 0.2}, {1.0, 1.0});
    const PriorBlock good_rtn = *PriorBlock::rtn({"p1", "p2", "p3"}, {0.0, 0.0, 0.0}, B, {0.2, 200.0, 0.2});

    // EST-F-401: an unknown name; a repeated name (by the factory); an RTN block of two names (assembled by hand, then validated by the consumer)
    refused_add(*PriorBlock::isotropic({"zz"}, {0.0}, {1.0}), good_iso, "EST-F-401", "zz");
    {
        const auto r = PriorBlock::isotropic({"p0", "p0"}, {0.0, 0.0}, {1.0, 1.0});
        REQUIRE(!r.has_value());
        CHECK(std::string(r.error().id) == "EST-F-401");
        CHECK(r.error().message.find("p0") != std::string::npos);
        PriorBlock hand;
        hand.names = {"p1", "p2"};
        hand.centre = {0.0, 0.0};
        hand.sigma = {0.2, 200.0, 0.2};
        hand.basis = B;
        refused_add(hand, good_iso, "EST-F-401", "RTN block");
    }
    // EST-F-402: sigma = 0, < 0, NaN, 2^-501, 2^501, a non-finite centre
    {
        const double bad_sigma[5] = {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), 0x1p-501, 0x1p+501};
        for (const double s : bad_sigma) {
            const auto r = PriorBlock::isotropic({"p0"}, {0.0}, {s});
            REQUIRE(!r.has_value());
            CHECK(std::string(r.error().id) == "EST-F-402");
            CHECK(r.error().message.find("p0") != std::string::npos);
        }
        const auto rc = PriorBlock::isotropic({"p0"}, {std::numeric_limits<double>::infinity()}, {1.0});
        REQUIRE(!rc.has_value());
        CHECK(std::string(rc.error().id) == "EST-F-402");
        // consumers validate again: a block assembled by hand with sigma = 0 is refused by add_prior
        PriorBlock hand;
        hand.names = {"p0"};
        hand.centre = {0.0};
        hand.sigma = {0.0};
        refused_add(hand, good_iso, "EST-F-402", "p0");
    }
    // EST-F-403: a scaled basis 2B; a non-finite entry; one entry perturbed by 1e-9
    {
        std::array<double, 9> twice = B;
        for (double& v : twice) v *= 2.0;
        std::array<double, 9> nonfinite = B;
        nonfinite[4] = std::numeric_limits<double>::quiet_NaN();
        std::array<double, 9> nudged = B;
        nudged[0] += 1.0e-9;   // NOT-A-UNIT-CROSSING: the registered perturbation, a dimensionless entry
        for (const auto& bad : {twice, nonfinite, nudged}) {
            const auto r = PriorBlock::rtn({"p1", "p2", "p3"}, {0.0, 0.0, 0.0}, bad, {0.2, 200.0, 0.2});
            REQUIRE(!r.has_value());
            CHECK(std::string(r.error().id) == "EST-F-403");
            CHECK(r.error().message.find("RTN block") != std::string::npos);
        }
        PriorBlock hand = good_rtn;
        hand.basis = twice;
        refused_add(hand, good_iso, "EST-F-403", "RTN block");
    }
    // EST-F-405: one parameter in an isotropic and an RTN block, both blocks named
    {
        auto ne = make_ne(names, d);
        REQUIRE(ne.add_prior(good_iso, x_lin).has_value());                            // constrains p0, p1
        const auto before = ne.formed_system();
        const auto r = ne.add_prior(good_rtn, x_lin);                                  // names p1 again
        REQUIRE(!r.has_value());
        CHECK(std::string(r.error().id) == "EST-F-405");
        CHECK(r.error().message.find("isotropic block") != std::string::npos);
        CHECK(r.error().message.find("RTN block") != std::string::npos);
        CHECK(r.error().message.find("p1") != std::string::npos);
        const auto after = ne.formed_system();
        CHECK(before->matrix == after->matrix);
        CHECK(before->rhs == after->rhs);
        // with_priors names both blocks too
        const auto w = with_priors(decay405(), {"a", "b"}, {*PriorBlock::isotropic({"a"}, {0.0}, {1.0}), *PriorBlock::isotropic({"a", "b"}, {0.0, 0.0}, {1.0, 1.0})});
        REQUIRE(!w.has_value());
        CHECK(std::string(w.error().id) == "EST-F-405");
    }
    // EST-F-406: SolveOptions::eliminate naming a constrained parameter, at solve()
    {
        auto ne = make_ne(names, d);
        REQUIRE(ne.add_prior(good_iso, x_lin).has_value());
        SolveOptions so;
        so.eliminate.push_back(Elimination{"p1", 0.3});
        const auto r = ne.solve(so);
        REQUIRE(!r.has_value());
        CHECK(std::string(r.error().id) == "EST-F-406");
        CHECK(r.error().message.find("p1") != std::string::npos);
        SolveOptions ok;
        ok.eliminate.push_back(Elimination{"p3", 0.3});
        CHECK(ne.solve(ok).has_value());
    }
    // EST-F-404 is EST-A-405's; observability (EST-R-406): a parameter no row observes and no block constrains is EST-F-201; with one isotropic block it solves and returns the centre
    {
        Data e;
        e.n = 2;
        e.m = 6;
        for (std::size_t k = 0; k < e.m; ++k) {
            e.A.push_back(g.uni());
            e.A.push_back(0.0);                                                          // no row observes the second parameter
            e.r.push_back(g.uni());
            e.sigma.push_back(1.0);
        }
        auto ne0 = make_ne({"o0", "o1"}, e);
        const auto r0 = ne0.solve();
        REQUIRE(!r0.has_value());
        CHECK(std::string(r0.error().id) == "EST-F-201");
        CHECK(r0.error().message.find("o1") != std::string::npos);
        auto ne1 = make_ne({"o0", "o1"}, e);
        const PriorBlock only = *PriorBlock::isotropic({"o1"}, {0.75}, {1.0});
        REQUIRE(ne1.add_prior(only, std::vector<double>(2, 0.0)).has_value());
        const auto r1 = ne1.solve();
        REQUIRE(r1.has_value());
        const double B1 = r1->certified_relative_error;
        SAY("406observable the prior-only parameter: x = %.15g (centre 0.75), kappa_i %.6g, B %.3e", r1->x[1], r1->condition.parameter_conditions[1], B1);
        CHECK(std::fabs(r1->x[1] - 0.75) <= 4.0 * B1 * 0.75 + 1.0e-15);
        CHECK(std::isfinite(r1->condition.parameter_conditions[1]));
    }
    // control: sigma at the range's edges 2^-500 and 2^500 is ACCEPTED and solves
    for (const double edge : {0x1p-500, 0x1p+500}) {
        Data e;
        e.n = 2;
        e.m = 6;
        for (std::size_t k = 0; k < e.m; ++k) {
            e.A.push_back(g.uni());
            e.A.push_back(0.0);
            e.r.push_back(g.uni());
            e.sigma.push_back(1.0);
        }
        auto ne = make_ne({"o0", "o1"}, e);
        const auto blk = PriorBlock::isotropic({"o1"}, {1.0}, {edge});
        REQUIRE(blk.has_value());
        REQUIRE(ne.add_prior(*blk, std::vector<double>(2, 0.0)).has_value());
        const auto sol = ne.solve();
        SAY("406edge sigma = %.3e: solve %.0f (1 = solved)", edge, sol.has_value() ? 1.0 : 0.0);
        CHECK(sol.has_value());
    }
}

// =====================================================================================================================================================================================
TEST_CASE("EST-A-407: the named case: a prediction wrong almost entirely along track, truth known; the isotropic constraint biases the along-track estimate, the RTN constraint does not", "[estimation][prior][a407]") {
    const std::array<double, 9> B = fixture_basis();
    const std::vector<std::string> names = {"px", "py", "pz"};
    const double sd_data[3] = {0.01, 10.0, 0.01};                                        // sigma_R, sigma_T, sigma_N of the tracking
    const double e_true[3] = {0.05, 100.0, 0.05};                                        // the truth: (dR, dT, dN) in metres

    // the data: three rows that measure the RTN components of the true error (row k is B_k, residual e_k, sigma sd_k)
    const auto build = [&](const double noise[3]) {
        Data d;
        d.n = 3;
        d.m = 3;
        for (std::size_t k = 0; k < 3; ++k) {
            for (std::size_t j = 0; j < 3; ++j) d.A.push_back(B[3 * k + j]);
            d.r.push_back(e_true[k] + (noise ? noise[k] * sd_data[k] : 0.0));
            d.sigma.push_back(sd_data[k]);
        }
        return d;
    };
    // the solve with a prior; returns the estimate's error in RTN components and the T-axis posterior standard deviation
    struct Out {
        double err[3];
        double sd_t;
        double Bcert;
        std::vector<double> x;
    };
    const auto solve_with = [&](const PriorBlock& blk, const double noise[3]) {
        const Data d = build(noise);
        auto ne = make_ne(names, d);
        REQUIRE(ne.add_prior(blk, std::vector<double>(3, 0.0)).has_value());
        const auto sol = ne.solve();
        REQUIRE(sol.has_value());
        Out o{};
        o.Bcert = sol->certified_relative_error;
        o.x = sol->x;
        // x_true (inertial) = B^T e_true; the error in RTN is B (x_hat - x_true)
        double xt[3] = {0, 0, 0};
        for (std::size_t j = 0; j < 3; ++j)
            for (std::size_t k = 0; k < 3; ++k) xt[j] += B[3 * k + j] * e_true[k];
        for (std::size_t k = 0; k < 3; ++k) {
            double s = 0.0;
            for (std::size_t j = 0; j < 3; ++j) s += B[3 * k + j] * (sol->x[j] - xt[j]);
            o.err[k] = s;
        }
        double v = 0.0;                                                                  // (B Q B^T)_TT
        for (std::size_t a = 0; a < 3; ++a)
            for (std::size_t b = 0; b < 3; ++b) v += B[3 + a] * sol->cofactor[a * 3 + b] * B[3 + b];
        o.sd_t = std::sqrt(v);
        return o;
    };
    const auto iso = *PriorBlock::isotropic(names, {0.0, 0.0, 0.0}, {0.2, 0.2, 0.2});
    const auto rtn = *PriorBlock::rtn({"px", "py", "pz"}, {0.0, 0.0, 0.0}, B, {0.2, 200.0, 0.2});
    const Out I = solve_with(iso, nullptr), R = solve_with(rtn, nullptr);

    // the predicted closed forms (registered)
    const double pI[3] = {-1.24688e-4, -99.9600, -1.24688e-4};
    const double pR[3] = {-1.24688e-4, -0.249377, -1.24688e-4};
    const auto rel_ok = [](double got, double want, double rel, double abs_floor_below, double abs_tol) {
        if (std::fabs(want) < abs_floor_below) return std::fabs(got - want) <= abs_tol;
        return std::fabs(got - want) <= rel * std::fabs(want);
    };
    // the registered values are printed to six figures; the gate is 1e-8 against the full-precision closed forms
    const ld nR = 1.0L / (0.01L * 0.01L), nT = 1.0L / (10.0L * 10.0L), lam = 25.0L, lamT = 1.0L / (200.0L * 200.0L);
    const ld fullI[3] = {-lam * 0.05L / (nR + lam), -lam * 100.0L / (nT + lam), -lam * 0.05L / (nR + lam)};
    const ld fullR[3] = {-lam * 0.05L / (nR + lam), -lamT * 100.0L / (nT + lamT), -lam * 0.05L / (nR + lam)};
    for (std::size_t k = 0; k < 3; ++k) {
        std::printf("PRIOR 407err axis %zu  isotropic %.9e (predicted %.9Le)  RTN %.9e (predicted %.9Le)\n", k, I.err[k], fullI[k], R.err[k], fullR[k]);
        CHECK(rel_ok(I.err[k], static_cast<double>(fullI[k]), 1.0e-8, 1.0e-6, 1.0e-10));
        CHECK(rel_ok(R.err[k], static_cast<double>(fullR[k]), 1.0e-8, 1.0e-6, 1.0e-10));
        CHECK(rel_ok(I.err[k], pI[k], 1.0e-5, 1.0e-6, 1.0e-10));   // the six-figure registered numbers themselves
        CHECK(rel_ok(R.err[k], pR[k], 1.0e-5, 1.0e-6, 1.0e-10));
    }
    const double ratio = std::fabs(I.err[1]) / std::fabs(R.err[1]);
    std::printf("PRIOR 407ratio isotropic/RTN along-track bias %.6f (predicted 400.840, bar > 10)\n", ratio);
    CHECK(ratio > 10.0);
    CHECK(rel_ok(ratio, 400.840, 1.0e-5, 0.0, 0.0));
    std::printf("PRIOR 407sd T-axis posterior sd: isotropic %.9g (predicted 0.199960012), RTN %.9g (predicted 9.98752339)\n", I.sd_t, R.sd_t);
    CHECK(rel_ok(I.sd_t, 0.199960011996, 1.0e-8, 0.0, 0.0));
    CHECK(rel_ok(R.sd_t, 9.987523388828, 1.0e-8, 0.0, 0.0));

    // controls (must fail against the RTN prediction): (i) B^T passed for B, (ii) RTN (0.2, 0.2, 0.2) equals the isotropic solve to B, (iii) RTN (0.2, 0.2, 200)
    {
        const auto bt = *PriorBlock::rtn({"px", "py", "pz"}, {0.0, 0.0, 0.0}, transposed(B), {0.2, 200.0, 0.2});
        const Out T = solve_with(bt, nullptr);
        std::printf("PRIOR 407control-i  B^T passed for B: along-track error %.9e (predicted -99.9600); against the RTN prediction -0.249377 the factor is %.3f\n", T.err[1], std::fabs(T.err[1]) / 0.249377);
        CHECK(rel_ok(T.err[1], static_cast<double>(fullI[1]), 1.0e-8, 0.0, 0.0));
        const double f = std::fabs(T.err[1]) / std::fabs(R.err[1]);
        CHECK((f > 399.0 && f < 402.0));
        const auto eq = *PriorBlock::rtn({"px", "py", "pz"}, {0.0, 0.0, 0.0}, B, {0.2, 0.2, 0.2});
        const Out E = solve_with(eq, nullptr);
        double dd = 0.0, nn = 0.0;
        for (std::size_t j = 0; j < 3; ++j) {
            dd += (E.x[j] - I.x[j]) * (E.x[j] - I.x[j]);
            nn += I.x[j] * I.x[j];
        }
        std::printf("PRIOR 407control-ii RTN (0.2, 0.2, 0.2) against the isotropic solve: relative difference %.3e (B %.3e, %.3e)\n", std::sqrt(dd / nn), E.Bcert, I.Bcert);
        CHECK(std::sqrt(dd / nn) <= E.Bcert + I.Bcert);
        const auto sw = *PriorBlock::rtn({"px", "py", "pz"}, {0.0, 0.0, 0.0}, B, {0.2, 0.2, 200.0});
        const Out S = solve_with(sw, nullptr);
        const ld lamN = 1.0L / (200.0L * 200.0L);
        const ld predN = -lamN * 0.05L / (nR + lamN);
        std::printf("PRIOR 407control-iii RTN (0.2, 0.2, 200): errors (%.9e, %.9e, %.9e); predicted (%.9e, -99.9600, %.4Le)\n", S.err[0], S.err[1], S.err[2], -1.24688e-4, predN);
        CHECK(rel_ok(S.err[0], static_cast<double>(fullI[0]), 1.0e-8, 1.0e-6, 1.0e-10));
        CHECK(rel_ok(S.err[1], static_cast<double>(fullI[1]), 1.0e-8, 0.0, 0.0));
        CHECK(rel_ok(S.err[2], static_cast<double>(predN), 1.0e-8, 1.0e-6, 1.0e-10));
    }

    // reported, not gated: 1000 replicates with noise from the data's own sigma; the mean error per axis beside the prediction and the standard error of the mean
    {
        Rng g(kSeed + 7);
        double sumI[3] = {0, 0, 0}, sumR[3] = {0, 0, 0};
        const int reps = 1000;   // NOT-A-UNIT-CROSSING: the registered replicate count
        for (int t = 0; t < reps; ++t) {
            const double noise[3] = {g.normal(), g.normal(), g.normal()};
            const Out a = solve_with(iso, noise), b = solve_with(rtn, noise);
            for (int k = 0; k < 3; ++k) {
                sumI[k] += a.err[k];
                sumR[k] += b.err[k];
            }
        }
        const double seI[3] = {0.0099875 / std::sqrt(1000.0), 0.199960012 / std::sqrt(1000.0), 0.0099875 / std::sqrt(1000.0)};   // NOT-A-UNIT-CROSSING: sqrt of the replicate count
        const double seR[3] = {0.0099875 / std::sqrt(1000.0), 9.98752339 / std::sqrt(1000.0), 0.0099875 / std::sqrt(1000.0)};     // NOT-A-UNIT-CROSSING: the same
        for (int k = 0; k < 3; ++k) {
            const double mI = sumI[k] / reps, mR = sumR[k] / reps;
            std::printf("PRIOR 407replicates (reported) axis %d: isotropic mean error %.6e (predicted %.6Le, s.e. %.4e, bias in s.e. %.2f)   RTN mean error %.6e (predicted %.6Le, s.e. %.4e, bias in s.e. %.2f)\n", k, mI,
                        fullI[k], seI[k], std::fabs(static_cast<double>(fullI[k])) / seI[k], mR, fullR[k], seR[k], std::fabs(static_cast<double>(fullR[k])) / seR[k]);
        }
    }
}
