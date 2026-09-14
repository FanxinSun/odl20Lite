/*! @file State_vector.h
	@author Santosh Bhattarai
	@date 16 February 2015
	@brief SGNL OPS file defining the class State_vector. This is an object for
		   dealing with orbits represeted in the state-vector (pos, vel) form.
 */

//!!!! WARNING: Do not directly include this file
//!!!! Instead, include Keplerian_elements.h

#ifndef SGNL_STATE_VECTOR_H
#define SGNL_STATE_VECTOR_H

#include "Cartesian.h"
#include "Timetag.h"

/**
 * @class State_vector
 * @author Santosh Bhattarai
 * @date 16 January 2015
 *
 * @brief This is a class that deals with orbits represeted in the state-vector
 *  	  format.
 *
 * None of the arithmetic operator overloads for this class affect the epoch.
 */
class State_vector
{
  public:
    // Attributes
    Timetag epoch;
    long double x;
    long double y;
    long double z;
    long double u;
    long double v;
    long double w;

    constexpr State_vector()
        : x{0.0L}, y{0.0L}, z{0.0L}, u{0.0L}, v{0.0L}, w{0.0L}
    {
    }

    constexpr State_vector(long double in_x, long double in_y, long double in_z,
                           long double in_u, long double in_v, long double in_w,
                           Timetag in_epoch)
        : epoch{in_epoch}, x{in_x}, y{in_y}, z{in_z}, u{in_u}, v{in_v}, w{in_w}
    {
    }

    constexpr State_vector(long double in_x, long double in_y, long double in_z,
                           double in_u, double in_v, double in_w,
                           Timetag in_epoch)
        : epoch{in_epoch}, x{in_x}, y{in_y}, z{in_z},
          u{static_cast<long double>(in_u)}, v{static_cast<long double>(in_v)},
          w{static_cast<long double>(in_w)}
    {
    }

    constexpr State_vector(double in_x, double in_y, double in_z, double in_u,
                           double in_v, double in_w, Timetag in_epoch)
        : epoch{in_epoch}, x{static_cast<long double>(in_x)},
          y{static_cast<long double>(in_y)}, z{static_cast<long double>(in_z)},
          u{static_cast<long double>(in_u)}, v{static_cast<long double>(in_v)},
          w{static_cast<long double>(in_w)}
    {
    }

    void set(Cartesian in_position, Cartesian in_velocity, Timetag in_epoch)
    {
        epoch = in_epoch;
        set(in_position.x, in_position.y, in_position.z, in_velocity.x,
            in_velocity.y, in_velocity.z);
    }

    void set(double in_x, double in_y, double in_z, double in_u, double in_v,
             double in_w, Timetag in_epoch)
    {
        epoch = in_epoch;
        set(in_x, in_y, in_z, in_u, in_v, in_w);
    }

    void set(long double in_x, long double in_y, long double in_z, double in_u,
             double in_v, double in_w, Timetag in_epoch)
    {
        epoch = in_epoch;
        set(in_x, in_y, in_z, in_u, in_v, in_w);
    }

    void set(long double in_x, long double in_y, long double in_z,
             long double in_u, long double in_v, long double in_w,
             Timetag in_epoch)
    {
        epoch = in_epoch;
        set(in_x, in_y, in_z, in_u, in_v, in_w);
    }

    void set(double in_x, double in_y, double in_z, double in_u, double in_v,
             double in_w)
    {
        set(static_cast<long double>(in_x), static_cast<long double>(in_y),
            static_cast<long double>(in_z), static_cast<long double>(in_u),
            static_cast<long double>(in_v), static_cast<long double>(in_w));
    }

    void set(long double in_x, long double in_y, long double in_z, double in_u,
             double in_v, double in_w)
    {
        set(in_x, in_y, in_z, static_cast<long double>(in_u),
            static_cast<long double>(in_v), static_cast<long double>(in_w));
    }

    void set(long double in_x, long double in_y, long double in_z,
             long double in_u, long double in_v, long double in_w)
    {
        x = in_x;
        y = in_y;
        z = in_z;
        u = in_u;
        v = in_v;
        w = in_w;
    }

    // Returns Cartesian, which only has double precision values
    Cartesian get_position() const
    {
        return Cartesian(x, y, z);
    }
    Cartesian get_velocity() const
    {
        return Cartesian(u, v, w);
    }

    long double dot_product() const
    {
        return x * u + y * v + z * w;
    }

    // Returns Cartesian, which only has double precision values
    Cartesian cross_product() const
    {
        long double cross_x = 0.0L;
        long double cross_y = 0.0L;
        long double cross_z = 0.0L;

        cross_product(cross_x, cross_y, cross_z);

        return Cartesian(cross_x, cross_y, cross_z);
    }

    // Populates arguments with full precision result
    void cross_product(long double &in_x, long double &in_y,
                       long double &in_z) const
    {
        in_x = y * w - z * v;
        in_y = z * u - x * w;
        in_z = x * v - y * u;
    }

    State_vector operator-() const
    {
        return State_vector(-x, -y, -z, -u, -v, -z, epoch);
    }

    void operator+=(State_vector a)
    {
        x += a.x;
        y += a.y;
        z += a.z;
        u += a.u;
        v += a.v;
        w += a.w;
    }

    void operator-=(State_vector a)
    {
        x -= a.x;
        y -= a.y;
        z -= a.z;
        u -= a.u;
        v -= a.v;
        w -= a.w;
    }

    void operator*=(long double k)
    {
        x *= k;
        y *= k;
        z *= k;
        u *= k;
        v *= k;
        w *= k;
    }

    void operator/=(long double k)
    {
        x /= k;
        y /= k;
        z /= k;
        u /= k;
        v /= k;
        w /= k;
    }

    //!< Generate representative string
    virtual std::string str() const; // Defined in Keplerian_elements

    //!< Print to screen (prints output of str())
    virtual void print() const; // Defined in Keplerian_elements

    // Rule of 5 since we need a virtual destructor
    virtual ~State_vector() = default;
    State_vector(const State_vector &copy_from) = default;
    State_vector &operator=(const State_vector &copy_from) = default;
    State_vector(State_vector &&) = default;
    State_vector &operator=(State_vector &&) = default;
};

inline State_vector operator+(State_vector a, State_vector b)
{
    a += b;
    return a;
}

inline State_vector operator-(State_vector a, State_vector b)
{
    a -= b;
    return a;
}

inline State_vector operator*(State_vector a, double k)
{
    a *= static_cast<long double>(k);
    return a;
}

inline State_vector operator*(double k, State_vector a)
{
    a *= static_cast<long double>(k);
    return a;
}

inline State_vector operator*(State_vector a, long double k)
{
    a *= k;
    return a;
}

inline State_vector operator*(long double k, State_vector a)
{
    a *= k;
    return a;
}

inline State_vector operator/(State_vector a, double k)
{
    a /= static_cast<long double>(k);
    return a;
}

inline State_vector operator/(State_vector a, long double k)
{
    a /= k;
    return a;
}

#endif
