#ifndef SGNL_LAGRANGE_INTERPOLATION_H
#define SGNL_LAGRANGE_INTERPOLATION_H

#include <vector>

/**  [contains the Lagrange polynomial first derivative to calculate, e.g. SV velocity]
    * Perform Lagrange interpolation (straightforward implementation) on the data (X[i],Y[i]), i=1,N (N=X.size()),
    * returning the value of Y(x) and dY(x)/dX by computing the Lagrange polynomial first derivative.
    * Assumes that x is between X[k-1] and X[k], where k=N/2.
    * This implementation is based on the reference http://numericalmethods.eng.usf.edu/topics/lagrange_method.html
    * [Last Accessed May-2008]
    * Also estimates velocity (accurate enough for GNSS application) or clock drift by taking the first derivative of
    * the "n" order Lagrange polynomial.
    *
    * Warning: for use with the precise (SP3) ephemeris only when velocity is not
    * available; estimates of velocity, and especially clock drift, not as accurate.
    */
// class T should be of a type with a single argument constructor that has the
// following operator overloads: *= += T/T T-T
// class U only needs the operator overload: +=
// The operator U * T should also be defined
template <class T, class U>
void lagrange_interpolation(const std::vector<T> &X, const std::vector<U> &Y,
                            const T &x, U &y, U &dydx)
{
    size_t N = X.size();

    size_t LD_size = (N * (N + 1)) / 2;
    std::vector<T> L(N, T(1)), M(N, T(1));
    std::vector<T> LD(LD_size, T(1));

    size_t i, j, k;
    /// Constructing L[i] = PROD(i != j)(x-X[j]), where L[i] is the numerator (top) in each Lagrange interpolation fractions and
    /// M[i] = PROD(i != j) (X[i] - X[j]), where M[i] is the denominator in each Lagrange interpolation fractions
    /// LD(i,j) = if( i < j ) then PROD(k!=i,k!=j)(x-X[k]) which is a matrix holding the numerators used to compute the derivatives.
    /// LD(i,j) is symmetric, there are only N(N+1)/2 - N of them, hence we store them in a vector of length N(N+1)/2,
    /// where LD(i,j) == LD[i+j*(j+1)/2] (ignoring i=j).
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            if (i != j) {
                L[i] *= x - X[j];
                M[i] *= X[i] - X[j];
                if (i < j) {
                    for (k = 0; k < N; k++) {
                        if (k == i || k == j)
                            continue;
                        LD[i + (j * (j + 1)) / 2] *= (x - X[k]);
                    }
                }
            }
        }
    }

    /// Computing the Y(x) using the straightforward implementation of the Lagrange polynomial as follow:
    /// Given y(x) is the function being approximated then y(x) = SUM(L[i](x) * Y[i]) where
    /// L[i](x) = L[i] / M[i].
    ///
    /// Also computes dY(x)/dx based on the Lagrange polynomial first derivative as follow:
    /// dy(x)/dx = SUM(Deriv[i](x) * Y[i]) where
    /// Deriv[i](x) = SUM((i != k) LD(i,k) / M[i])
    ///
    y = Y[0] * T(0); // zero out variable of type U
    dydx = y;
    for (i = 0; i < N; i++) {
        y += Y[i] * (L[i] / M[i]);
        T Deriv(0);

        for (k = 0; k < N; k++)
            if (i != k) {
                if (k < i)
                    Deriv += LD[k + (i * (i + 1)) / 2] / M[i];
                else
                    Deriv += LD[i + (k * (k + 1)) / 2] / M[i];
            }
        dydx += Y[i] * Deriv;
    }
}

#endif
