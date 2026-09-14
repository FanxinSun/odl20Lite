/*! @file Cartesian.h
 	@author Santosh Bhattarai
 	@date 31 March 2015
 	@brief SGNL OPS file defining the class Cartesian, a 3 dimensional vector.
 */

#ifndef SGNL_CARTESIAN_H
#define SGNL_CARTESIAN_H

#include <cmath>
#include <iostream>
#include <sstream>

class Cartesian
{
  public:
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Cartesian() = default;

    Cartesian(double in_x, double in_y, double in_z)
        : x{in_x}, y{in_y}, z{in_z}
    {
    }

    Cartesian(long double in_x, long double in_y, long double in_z)
        : x{static_cast<double>(in_x)}, y{static_cast<double>(in_y)},
          z{static_cast<double>(in_z)}
    {
    }

    Cartesian(long double in_x, long double in_y, long double in_z,
                        long double mag)
        : x{static_cast<double>(in_x / mag)},
          y{static_cast<double>(in_y / mag)}, z{static_cast<double>(in_z / mag)}
    {
    }

    // Assignment method
    void set(double in_x, double in_y, double in_z)
    {
        x = in_x;
        y = in_y;
        z = in_z;
    }

    void set(long double in_x, long double in_y, long double in_z)
    {
        x = static_cast<double>(in_x);
        y = static_cast<double>(in_y);
        z = static_cast<double>(in_z);
    }

    void set(long double in_x, long double in_y, long double in_z,
             long double mag)
    {
        x = static_cast<double>(in_x / mag);
        y = static_cast<double>(in_y / mag);
        z = static_cast<double>(in_z / mag);
    }

    // Operator method, negation
    Cartesian operator-() const
    {
        return Cartesian(-x, -y, -z);
    }

    // Operator method, addition
    void operator+=(Cartesian a)
    {
        x += a.x;
        y += a.y;
        z += a.z;
    }

    // Operator method, subtraction
    void operator-=(Cartesian a)
    {
        x -= a.x;
        y -= a.y;
        z -= a.z;
    }

    // Operator method, scalar multiply
    void operator*=(double k)
    {
        x *= k;
        y *= k;
        z *= k;
    }

    // Operator method, scalar divide
    void operator/=(double k)
    {
        x /= k;
        y /= k;
        z /= k;
    }

    double length2() const
    {
        return x * x + y * y + z * z;
    }

    double length() const
    {
        return std::sqrt(length2());
    }

    // Method for scaling components so that the vector has magnitude 1.
    void normalise()
    {
        double s = length();
        if (s > 0.0) {
            x /= s;
            y /= s;
            z /= s;
        } else {
            x = 1.0;
            y = 0.0;
            z = 0.0;
        }
    }

    std::string str() const //!< Generate representative string
    {
        std::stringstream out;
        out.precision(16);

        out << "x: " << x << "\n"
            << "y: " << y << "\n"
            << "z: " << z;

        return out.str();
    }

    void print() const //!< Print to screen (prints output of str())
    {
        std::cout << str() << std::endl;
    }
};

// Method for scaling components so that the vector has magnitude 1.
inline Cartesian normalise(Cartesian a)
{
    a.normalise();
    return a;
}

/*
 * Further Cartesian (vector) operator functions:
 * 	1. Add two (3-)vectors. Usage: c = a + b;
 *  2. Subtract two vectors. Usage: c = a - b;
 *  3. Scalar multiplication. Usage: c = a(vector) * b(scalar)
 *  4. Scalar multiplication. Usage: c = b(scalar) * a(vector)
 *  5. Scalar division. Usage: c = a(vector)/b(scalar)
 */
inline Cartesian operator+(Cartesian a, Cartesian b)
{
    a += b;
    return a;
}

inline Cartesian operator-(Cartesian a, Cartesian b)
{
    a -= b;
    return a;
}

inline Cartesian operator*(Cartesian a, double b)
{
    a *= b;
    return a;
}

inline Cartesian operator*(double b, Cartesian a)
{
    a *= b;
    return a;
}

inline Cartesian operator/(Cartesian a, double b)
{
    a /= b;
    return a;
}

inline Cartesian cross_product(const Cartesian &a, const Cartesian &b)
{
    return Cartesian(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}

inline double dot_product(const Cartesian &a, const Cartesian &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

#endif
