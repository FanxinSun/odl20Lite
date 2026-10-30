#pragma once
// nielsen.hpp — Nielsen's test problems (H. B. Nielsen, "Damping Parameter in Marquardt's Method", IMM-REP-1999-05, DTU 1999, the Appendix: "Test Problems") as the HELD-OUT test of L7 step 3.
// Written from the Appendix's own pages, READ AS IMAGES (the text layer breaks the formulae: rotated pages, two columns), each problem then checked against the minimum the Appendix prints
// (lm_nielsen_tests.cpp, EST-A-330) before any fit used it.  The first 17 problems are the Argonne set (Moré, Garbow & Hillstrom) the report cites as [4]; nothing here is taken from that set
// or from memory: where the Appendix does not give what a problem needs, the problem is OMITTED and recorded (Problem 17: its table of responses prints the abscissae t_i = 10 (i-1) in the
// y column, so Osborne 1's responses are not in the document).
// The instances are the 30 rows of the report's Tables 3.3a-b: (Pno, m, n), with the report's starting point and the final F it prints (three digits).
#include "dual.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace odl::estimation::fx {

struct NielsenCase {
    int pno;
    std::size_t m;
    std::size_t n;
    double f_printed;                                // the F(x*) of Table 3.3b (three digits); 0 for the zero-residual problems
};

inline const std::vector<NielsenCase>& nielsen_cases() {
    static const std::vector<NielsenCase> v = {
        {1, 8, 8, 0.0},      {1, 32, 16, 8.00},   {2, 8, 8, 0.824},   {2, 32, 16, 3.82},  {3, 8, 8, 1.58},      {3, 32, 16, 4.57},    {4, 2, 2, 0.0},       {5, 3, 3, 0.0},
        {6, 4, 4, 0.0},      {7, 2, 2, 24.5},     {8, 15, 3, 4.11e-3}, {9, 11, 4, 1.54e-4}, {10, 16, 3, 44.0},  {11, 31, 6, 1.14e-3}, {11, 31, 9, 7.00e-7}, {11, 31, 12, 2.36e-10},
        {12, 5, 3, 0.0},     {12, 10, 3, 0.0},    {13, 10, 2, 62.2},  {14, 20, 4, 4.29e4}, {15, 8, 8, 1.76e-3}, {15, 16, 8, 2.95e-2}, {15, 9, 9, 0.0},    {15, 18, 9, 3.55e-2},
        {16, 5, 5, 0.0},     {16, 10, 10, 0.0},   {17, 33, 5, 2.73e-5}, {18, 45, 4, 5.00e-3}, {19, 45, 2, 5.00e-3}, {20, 16, 3, 4.40e-5},
    };
    return v;
}

/// Problem 17's responses are not in the document: the instance is listed and not run.
inline bool nielsen_available(int pno) { return pno != 17; }

inline const double kMeyerY[16] = {34780, 28610, 23650, 19630, 16370, 13720, 11540, 9744, 8261, 7030, 6005, 5147, 4427, 3820, 3307, 2872};
inline const double kBardY[15] = {0.14, 0.18, 0.22, 0.25, 0.29, 0.32, 0.35, 0.39, 0.37, 0.58, 0.73, 0.96, 1.34, 2.10, 4.39};
inline const double kKowalikY[11] = {0.1957, 0.1947, 0.1735, 0.1600, 0.0844, 0.0627, 0.0456, 0.0342, 0.0323, 0.0235, 0.0246};
inline const double kKowalikU[11] = {4.0000, 2.0000, 1.0000, 0.5000, 0.2500, 0.1670, 0.1250, 0.1000, 0.0833, 0.0714, 0.0625};
inline const double kExpFitY[45] = {0.090542, 0.124569, 0.179367, 0.195654, 0.269707, 0.286027, 0.289892, 0.317475, 0.308191, 0.336995, 0.348371, 0.321337, 0.299423, 0.338972, 0.304763,
                                    0.288903, 0.300820, 0.303974, 0.283987, 0.262078, 0.281593, 0.267531, 0.218926, 0.225572, 0.200594, 0.197375, 0.182440, 0.183892, 0.152285, 0.174028,
                                    0.150874, 0.126220, 0.126266, 0.106384, 0.118923, 0.091868, 0.128926, 0.119273, 0.115997, 0.105831, 0.075261, 0.068387, 0.090823, 0.085205, 0.067203};

/// The starting point the Appendix gives (Chebyquad's and the linear problems' depend on n).
inline std::vector<double> nielsen_x0(int pno, std::size_t n) {
    switch (pno) {
        case 1: case 2: case 3: case 8: return std::vector<double>(n, 1.0);
        case 4: return {-1.2, 1.0};
        case 5: return {-1.0, 0.0, 0.0};
        case 6: return {3.0, -1.0, 0.0, 1.0};
        case 7: return {0.5, -2.0};
        case 9: return {0.25, 0.39, 0.415, 0.39};
        case 10: return {0.02, 4000.0, 250.0};
        case 11: return std::vector<double>(n, 0.0);
        case 12: return {0.0, 10.0, 20.0};
        case 13: return {0.3, 0.4};
        case 14: return {25.0, 5.0, -5.0, -1.0};
        case 15: {
            std::vector<double> x(n);
            for (std::size_t j = 0; j < n; ++j) x[j] = static_cast<double>(j + 1) / static_cast<double>(n + 1);
            return x;
        }
        case 16: return std::vector<double>(n, 0.5);
        case 18: return {-1.0, -2.0, 1.0, -1.0};
        case 19: return {-1.0, -2.0};
        case 20: return {8.85, 4.0, 2.5};
        default: return {};
    }
}

/// The residual functions f_i(x), i = 1..m, of the Appendix, as written there; the least-squares objective is F = (1/2) sum f_i^2.  A template over the number type (double, Dual).
template <class T>
void nielsen_f(int pno, std::size_t m, std::size_t n, const T* x, T* f) {
    const double pi = 3.141592653589793238462643383279;
    const double dm = static_cast<double>(m);
    switch (pno) {
        case 1: {                                    // A = [I - (2/m) E ; -(2/m) E], f = A x - e
            T s = 0.0;
            for (std::size_t j = 0; j < n; ++j) s = s + x[j];
            for (std::size_t i = 0; i < m; ++i) f[i] = (i < n ? x[i] : T(0.0)) - (2.0 / dm) * s - 1.0;
            break;
        }
        case 2: {                                    // A = [1..m]^T [1..n]
            T s = 0.0;
            for (std::size_t j = 0; j < n; ++j) s = s + static_cast<double>(j + 1) * x[j];
            for (std::size_t i = 0; i < m; ++i) f[i] = static_cast<double>(i + 1) * s - 1.0;
            break;
        }
        case 3: {                                    // rows 1 and m and columns 1 and n are zero; A_ab = (a-1) b for a = 2..m-1, b = 2..n-1
            T s = 0.0;
            for (std::size_t b = 2; b + 1 <= n; ++b) s = s + static_cast<double>(b) * x[b - 1];
            f[0] = T(-1.0);
            f[m - 1] = T(-1.0);
            for (std::size_t a = 2; a + 1 <= m; ++a) f[a - 1] = static_cast<double>(a - 1) * s - 1.0;
            break;
        }
        case 4:
            f[0] = 10.0 * (x[1] - x[0] * x[0]);
            f[1] = 1.0 - x[0];
            break;
        case 5: {
            const double c = value_of(x[0]) > 0.0 ? 0.0 : 5.0;
            f[0] = 10.0 * (x[2] - (5.0 / pi) * atan(x[1] / x[0]) - c);
            f[1] = 10.0 * (sqrt(x[0] * x[0] + x[1] * x[1]) - 1.0);
            f[2] = x[2];
            break;
        }
        case 6:
            f[0] = x[0] + 10.0 * x[1];
            f[1] = std::sqrt(5.0) * (x[2] - x[3]);
            f[2] = (x[1] - 2.0 * x[2]) * (x[1] - 2.0 * x[2]);
            f[3] = std::sqrt(10.0) * (x[0] - x[3]) * (x[0] - x[3]);
            break;
        case 7:
            f[0] = x[0] - x[1] * (2.0 - x[1] * (5.0 - x[1])) - 13.0;
            f[1] = x[0] - x[1] * (14.0 - x[1] * (1.0 + x[1])) - 29.0;
            break;
        case 8:
            for (std::size_t i = 0; i < m; ++i) {
                const double u = static_cast<double>(i + 1), v = 16.0 - u, w = std::min(u, v);
                f[i] = kBardY[i] - (x[0] + u / (x[1] * v + x[2] * w));
            }
            break;
        case 9:
            for (std::size_t i = 0; i < m; ++i) {
                const double u = kKowalikU[i];
                f[i] = kKowalikY[i] - x[0] * u * (u + x[1]) / (u * (u + x[2]) + x[3]);
            }
            break;
        case 10:
            for (std::size_t i = 0; i < m; ++i) {
                const double t = 45.0 + 5.0 * static_cast<double>(i + 1);
                f[i] = x[0] * exp(x[1] / (t + x[2])) - kMeyerY[i];
            }
            break;
        case 11:
            for (std::size_t i = 0; i < 29; ++i) {
                const double t = static_cast<double>(i + 1) / 29.0;
                T d = 0.0, y = 0.0;
                for (std::size_t j = 1; j < n; ++j) d = d + static_cast<double>(j) * std::pow(t, static_cast<double>(j - 1)) * x[j];   // j here is 1-based j-1 of the sum j = 2..n
                for (std::size_t j = 0; j < n; ++j) y = y + std::pow(t, static_cast<double>(j)) * x[j];
                f[i] = d - y * y - 1.0;
            }
            f[29] = x[0];
            f[30] = x[1] - x[0] * x[0] - 1.0;
            break;
        case 12:
            for (std::size_t i = 0; i < m; ++i) {
                const double t = static_cast<double>(i + 1) / 10.0;
                f[i] = exp(-x[0] * t) - exp(-x[1] * t) - x[2] * (std::exp(-t) - std::exp(-10.0 * t));
            }
            break;
        case 13:
            for (std::size_t i = 0; i < m; ++i) {
                const double k = static_cast<double>(i + 1);
                f[i] = 2.0 + 2.0 * k - (exp(k * x[0]) + exp(k * x[1]));
            }
            break;
        case 14:
            for (std::size_t i = 0; i < m; ++i) {
                const double t = static_cast<double>(i + 1) / 5.0;
                const T a = x[0] + x[1] * t - std::exp(t);
                const T b = x[2] + x[3] * std::sin(t) - std::cos(t);
                f[i] = a * a + b * b;
            }
            break;
        case 15:
            for (std::size_t i = 0; i < m; ++i) f[i] = T(0.0);
            for (std::size_t j = 0; j < n; ++j) {
                const T z = 2.0 * x[j] - 1.0;
                T tkm = T(1.0), tk = z;              // T_0, T_1
                for (std::size_t i = 0; i < m; ++i) {
                    f[i] = f[i] + tk / static_cast<double>(n);
                    const T next = 2.0 * z * tk - tkm;
                    tkm = tk;
                    tk = next;
                }
            }
            for (std::size_t i = 0; i < m; ++i) {
                const std::size_t k = i + 1;
                const double y = (k % 2 == 1) ? 0.0 : -1.0 / (static_cast<double>(k * k) - 1.0);
                f[i] = f[i] - y;
            }
            break;
        case 16: {
            T s = 0.0, p = 1.0;
            for (std::size_t j = 0; j < n; ++j) { s = s + x[j]; p = p * x[j]; }
            for (std::size_t i = 0; i + 1 < n; ++i) f[i] = x[i] + s - static_cast<double>(n + 1);
            f[n - 1] = p - 1.0;
            break;
        }
        case 18:
            for (std::size_t i = 0; i < m; ++i) {
                const double t = 0.02 * static_cast<double>(i + 1);
                f[i] = kExpFitY[i] - (x[2] * exp(x[0] * t) + x[3] * exp(x[1] * t));   // the exponents are +x t: the minimiser [-4, -5, 4, -4] and the start [-1, -2, 1, -1] are decays
            }
            break;
        case 19: {                                   // the linear coefficients are the least-squares solution of a 2 x 2 system, differentiated through
            T a11 = 0.0, a12 = 0.0, a22 = 0.0, b1 = 0.0, b2 = 0.0;
            std::vector<T> e1(m), e2(m);
            for (std::size_t i = 0; i < m; ++i) {
                const double t = 0.02 * static_cast<double>(i + 1);
                e1[i] = exp(x[0] * t);
                e2[i] = exp(x[1] * t);
                a11 = a11 + e1[i] * e1[i];
                a12 = a12 + e1[i] * e2[i];
                a22 = a22 + e2[i] * e2[i];
                b1 = b1 + e1[i] * kExpFitY[i];
                b2 = b2 + e2[i] * kExpFitY[i];
            }
            const T det = a11 * a22 - a12 * a12;
            const T c1 = (a22 * b1 - a12 * b2) / det;
            const T c2 = (a11 * b2 - a12 * b1) / det;
            for (std::size_t i = 0; i < m; ++i) f[i] = kExpFitY[i] - (c1 * e1[i] + c2 * e2[i]);
            break;
        }
        case 20:
            for (std::size_t i = 0; i < m; ++i) {
                const double t = 0.45 + 0.05 * static_cast<double>(i + 1);
                f[i] = x[0] * exp(10.0 * x[1] / (t + x[2]) - 13.0) - 1.0e-3 * kMeyerY[i];
            }
            break;
        default: break;
    }
}

}  // namespace odl::estimation::fx
