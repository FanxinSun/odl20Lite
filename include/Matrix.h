/*! @file Matrix.h
 	@author David Harrison
 	@date 24 March 2017
 	@brief Supporting functions for the Matrix class in the Eigen library.
           The actual Matrix class is defined in the Eigen code found in the
           external/Eigen directory.
 */

#ifndef SGNL_MATRIX_H
#define SGNL_MATRIX_H

// Disable compiler warnings for Eigen library
// clang-format off
#ifdef __clang__

  #pragma clang diagnostic push

  #pragma clang diagnostic ignored "-Wshadow"
  #pragma clang diagnostic ignored "-Wconversion"
  #pragma clang diagnostic ignored "-Wextra-semi"
  #pragma clang diagnostic ignored "-Wdeprecated"
  #pragma clang diagnostic ignored "-Wfloat-equal"
  #pragma clang diagnostic ignored "-Wdocumentation"
  #pragma clang diagnostic ignored "-Wold-style-cast"
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #pragma clang diagnostic ignored "-Wused-but-marked-unused"
  #pragma clang diagnostic ignored "-Wdocumentation-unknown-command"

#elif defined __GNUC__

  #pragma GCC diagnostic push

  #pragma GCC diagnostic ignored "-Wshadow"
  #pragma GCC diagnostic ignored "-Wconversion"

#endif

#include "../external/Eigen/Dense"

// Re-enable compiler warnings for OPS code
#ifdef __clang__
  #pragma clang diagnostic pop
#elif defined __GNUC__
  #pragma GCC diagnostic pop
#endif
// clang-format on

#include "Keplerian_elements.h"

typedef Eigen::Matrix<double, 3, 3> Matrix3x3;
typedef Eigen::Matrix<double, 6, 6, Eigen::DontAlign> Matrix6x6;

typedef Eigen::Matrix<double, 3, 1> Matrix3x1;
typedef Eigen::Matrix<double, 6, 1> Matrix6x1;

template <typename Scalar>
inline Eigen::Quaternion<Scalar> rotate_around_x(Scalar alpha)
{
    return Eigen::Quaternion<Scalar>(
        Eigen::AngleAxis<Scalar>(alpha, Eigen::Matrix<Scalar, 3, 1>::UnitX()));
}
template <typename Scalar>
inline Eigen::Quaternion<Scalar> rotate_around_y(Scalar alpha)
{
    return Eigen::Quaternion<Scalar>(
        Eigen::AngleAxis<Scalar>(alpha, Eigen::Matrix<Scalar, 3, 1>::UnitY()));
}
template <typename Scalar>
inline Eigen::Quaternion<Scalar> rotate_around_z(Scalar alpha)
{
    return Eigen::Quaternion<Scalar>(
        Eigen::AngleAxis<Scalar>(alpha, Eigen::Matrix<Scalar, 3, 1>::UnitZ()));
}

// The Eigen library already includes a function to convert a Quaternion to an
// equivalent Matrix, however it assumes that the Quaternion length is exactly 1
// which is rarely ever the case when floating point arithmatic is involved.
// This method does not make that assumption, and we can correct the resulting
// matrix afterward if need be.
template <typename Scalar>
inline Eigen::Matrix<Scalar, 3, 3> get_matrix(Eigen::Quaternion<Scalar> q)
{
    Scalar w = q.w();
    Scalar x = q.x();
    Scalar y = q.y();
    Scalar z = q.z();

    Scalar ww = w * w;
    Scalar xx = x * x;
    Scalar yy = y * y;
    Scalar zz = z * z;

    w *= Scalar(2);
    Scalar wx = w * x;
    Scalar wy = w * y;
    Scalar wz = w * z;

    x *= Scalar(2);
    Scalar xy = x * y;
    Scalar xz = x * z;

    Scalar yz = Scalar(2) * y * z;

    Eigen::Matrix<Scalar, 3, 3> output;

    output(0, 0) = ww + xx - yy - zz;
    output(0, 1) = xy - wz;
    output(0, 2) = xz + wy;

    output(1, 0) = xy + wz;
    output(1, 1) = ww - xx + yy - zz;
    output(1, 2) = yz - wx;

    output(2, 0) = xz - wy;
    output(2, 1) = yz + wx;
    output(2, 2) = ww - xx - yy + zz;

    return output;
}

inline Matrix3x3 get_matrix(Cartesian row1, Cartesian row2, Cartesian row3)
{
    Matrix3x3 output;

    // clang-format off
    output << row1.x, row1.y, row1.z,
              row2.x, row2.y, row2.z,
              row3.x, row3.y, row3.z;
    // clang-format on

    return output;
}

inline Matrix6x1 convert_vector(const State_vector &a)
{
    Matrix6x1 output;

    // clang-format off
    output << static_cast<double>(a.x),
              static_cast<double>(a.y),
              static_cast<double>(a.z),
              static_cast<double>(a.u),
              static_cast<double>(a.v),
              static_cast<double>(a.w);
    // clang-format on

    return output;
}

inline Matrix3x1 convert_position(const State_vector &a)
{
    Matrix3x1 output;

    // clang-format off
    output << static_cast<double>(a.x),
              static_cast<double>(a.y),
              static_cast<double>(a.z);
    // clang-format on

    return output;
}

inline Matrix3x1 convert_velocity(const State_vector &a)
{
    Matrix3x1 output;

    // clang-format off
    output << static_cast<double>(a.u),
              static_cast<double>(a.v),
              static_cast<double>(a.w);
    // clang-format on

    return output;
}

inline Matrix3x3 skew_symmetric(Cartesian a)
{
    Matrix3x3 output;

    // clang-format off
    output <<  0.0, -a.z,  a.y,
               a.z,  0.0, -a.x,
              -a.y,  a.x,  0.0;
    // clang-format on

    return output;
}

inline Matrix3x3 diagonal(double a, double b, double c)
{
    Matrix3x3 output;

    // clang-format off
    output <<    a, 0.0, 0.0,
               0.0,   b, 0.0,
               0.0, 0.0,   c;
    // clang-format on

    return output;
}

inline Matrix3x3 diagonal(Cartesian a)
{
    return diagonal(a.x, a.y, a.z);
}

inline Matrix3x3 diagonal(double a)
{
    return diagonal(a, a, a);
}

// Column vector multiplied by row vector giving a 3x3 Matrix
inline Matrix3x3 col_times_row(Cartesian col, Cartesian row)
{
    Matrix3x3 output;

    // clang-format off
    output << col.x * row.x, col.x * row.y, col.x * row.z,
              col.y * row.x, col.y * row.y, col.y * row.z,
              col.z * row.x, col.z * row.y, col.z * row.z;
    // clang-format on

    return output;
}

// Matrix * column vector
inline Cartesian operator*(const Matrix3x3 &a, Cartesian b)
{
    Cartesian result;
    result.x = a(0, 0) * b.x + a(0, 1) * b.y + a(0, 2) * b.z;
    result.y = a(1, 0) * b.x + a(1, 1) * b.y + a(1, 2) * b.z;
    result.z = a(2, 0) * b.x + a(2, 1) * b.y + a(2, 2) * b.z;

    return result;
}

// Row vector * matrix
inline Cartesian operator*(Cartesian a, const Matrix3x3 &b)
{
    Cartesian result;
    result.x = a.x * b(0, 0) + a.y * b(1, 0) + a.z * b(2, 0);
    result.y = a.x * b(0, 1) + a.y * b(1, 1) + a.z * b(2, 1);
    result.z = a.x * b(0, 2) + a.y * b(1, 2) + a.z * b(2, 2);

    return result;
}

// Matrix * 2 column vectors
inline State_vector operator*(const Matrix3x3 &a, const State_vector &b)
{
    Eigen::Matrix<long double, 3, 3> c = a.cast<long double>();

    State_vector result;
    result.epoch = b.epoch;

    result.x = c(0, 0) * b.x + c(0, 1) * b.y + c(0, 2) * b.z;
    result.y = c(1, 0) * b.x + c(1, 1) * b.y + c(1, 2) * b.z;
    result.z = c(2, 0) * b.x + c(2, 1) * b.y + c(2, 2) * b.z;

    result.u = c(0, 0) * b.u + c(0, 1) * b.v + c(0, 2) * b.w;
    result.v = c(1, 0) * b.u + c(1, 1) * b.v + c(1, 2) * b.w;
    result.w = c(2, 0) * b.u + c(2, 1) * b.v + c(2, 2) * b.w;

    return result;
}

// 2 row vectors * matrix
inline State_vector operator*(const State_vector &a, const Matrix3x3 &b)
{
    Eigen::Matrix<long double, 3, 3> c = b.cast<long double>();

    State_vector result;
    result.epoch = a.epoch;

    result.x = a.x * c(0, 0) + a.y * c(1, 0) + a.z * c(2, 0);
    result.y = a.x * c(0, 1) + a.y * c(1, 1) + a.z * c(2, 1);
    result.z = a.x * c(0, 2) + a.y * c(1, 2) + a.z * c(2, 2);

    result.u = a.u * c(0, 0) + a.v * c(1, 0) + a.w * c(2, 0);
    result.v = a.u * c(0, 1) + a.v * c(1, 1) + a.w * c(2, 1);
    result.w = a.u * c(0, 2) + a.v * c(1, 2) + a.w * c(2, 2);

    return result;
}

#endif
