/*! @file Polynomial.h
	@author David Harrison
	@date 13 September 2017
	@brief Various functions to generate and evaluate polynomials.
 */

#ifndef SGNL_POLYNOMIAL_H
#define SGNL_POLYNOMIAL_H

#include <array>
#include <valarray>
#include <vector>

#include "Cartesian.h"

// Usage is poly(x, a0, a1, a2, ...) and calculates a0 + a1 * x + a2 * x^2 + ...
// Variable x and polynomial coefficients need not be the same type, although
// the return type is the same as the coefficients type.
template <typename T, typename U> inline constexpr U poly(T x, U a0, U a1)
{
    return x * a1 + a0;
}

#ifdef FP_FAST_FMAL
inline constexpr long double poly(long double x, long double a0, long double a1)
{
    return std::fma(x, a1, a0);
}
#endif

#ifdef FP_FAST_FMA
inline double poly(double x, double a0, double a1)
{
    return std::fma(x, a1, a0);
}

inline Cartesian poly(double x, Cartesian a0, Cartesian a1)
{
    return Cartesian(poly(x, a0.x, a1.x), poly(x, a0.y, a1.y),
                     poly(x, a0.z, a1.z));
}
#endif

#ifdef FP_FAST_FMAF
inline float poly(float x, float a0, float a1)
{
    return std::fma(x, a1, a0);
}
#endif

template <typename T, typename U, typename... Args>
inline U poly(T x, U a0, Args... as)
{
    return poly(x, a0, poly(x, as...));
}

template <typename T, typename U, std::size_t N>
inline U poly(T x, std::array<U, N> a)
{
    static_assert(a.size() > 0,
                  "Zero length arrays shouldn't be passed to poly.");

    size_t n = a.size();

    U result = a[--n];

    while (n > 0) {
        result = poly(x, a[--n], result);
    }

    return result;
}

template <typename T, typename U> inline U poly(T x, std::vector<U> a)
{
    size_t n = a.size();

    if (n == 0) {
        return U(0);
    } else {
        U result = a[--n];

        while (n > 0) {
            result = poly(x, a[--n], result);
        }

        return result;
    }
}

template <typename T, typename U> inline U poly(T x, std::valarray<U> a)
{
    size_t n = a.size();

    if (n == 0) {
        return U(0);
    } else {
        U result = a[--n];

        while (n > 0) {
            result = poly(x, a[--n], result);
        }

        return result;
    }
}

template <typename U, std::size_t N>
inline std::array<U, N - 1> differentiate_poly(std::array<U, N> a)
{
    static_assert(a.size() > 1, "Arrays of length zero or one shouldn't be "
                                "passed to differentiate_poly.");

    constexpr size_t n = a.size();

    std::array<U, n - 1> result;

    size_t i = 0u;
    size_t j = 1u;
    double mul = 0.0;

    while (j < n) {
        mul += 1.0;
        result[i] = mul * a[j];
        i = j;
        ++j;
    }

    return result;
}

template <typename U> inline std::vector<U> differentiate_poly(std::vector<U> a)
{
    size_t n = a.size();

    if (n == 0 || n == 1) {
        std::vector<U> result(1u);
        return result;
    } else {
        std::vector<U> result(n - 1);

        size_t i = 0u;
        size_t j = 1u;
        double mul = 0.0;

        while (j < n) {
            mul += 1.0;
            result[i] = mul * a[j];
            i = j;
            ++j;
        }

        return result;
    }
}

template <typename U>
inline std::valarray<U> differentiate_poly(std::valarray<U> a)
{
    size_t n = a.size();

    if (n == 0 || n == 1) {
        std::valarray<U> result(1u);
        return result;
    } else {
        std::valarray<U> result(n - 1);

        size_t i = 0u;
        size_t j = 1u;
        double mul = 0.0;

        while (j < n) {
            mul += 1.0;
            result[i] = mul * a[j];
            i = j;
            ++j;
        }

        return result;
    }
}

// Calculates and returns the coefficients for Chebyshev polynomials of the
// first kind up to T_N
inline std::vector<std::valarray<double>>
get_chebyshev_polynomials(std::size_t N)
{
    N++;
    N = (2u > N) ? 2u : N;

    std::vector<std::valarray<double>> output(N, std::valarray<double>(N));

    output[0][0] = 1.0;

    output[1] = output[0].shift(-1);

    for (size_t n = 2; n < N; ++n) {
        output[n] = 2.0 * output[n - 1].shift(-1) - output[n - 2];
    }

    return output;
}

// Directly evaluates a series of Chebyshev polynomials of the first kind at
// point t with a given array of coefficients
template <typename T, typename U, std::size_t N>
inline Cartesian cheb(T t, std::array<U, N> coeffs)
{
    T twot = t + t;
    Cartesian fi, fi1, fi2;

    for (size_t i = N - 1; i > 0; --i) {
        fi = twot * fi1 + (coeffs[i] - fi2);
        fi2 = fi1;
        fi1 = fi;
    }

    return (t * fi1 + (coeffs[0] - fi2));
}

#endif
