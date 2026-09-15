/*! @file Tracking_fit.h
 *  @date 16 September 2026
 *  @brief Pieces shared by the fits that work from tracking measurements
 *         rather than from somebody else's trajectory.
 *
 *  fit_orbit_to_slr fits laser ranges; fit_orbit_to_angles fits optical right
 *  ascension and declination. What they have in common is everything except the
 *  measurement itself: where the observer is and how to get them into the
 *  inertial frame, how to read the propagated trajectory at an epoch that is
 *  not on the integration grid, and how to solve normal equations whose columns
 *  differ by twelve orders of magnitude.
 */

#ifndef SGNL_TRACKING_FIT_H
#define SGNL_TRACKING_FIT_H

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "Matrix.h"
#include "Propagators.h"

namespace sgnlTracking
{
struct Station {
    std::string id;
    Cartesian pos;    //!< ITRF position at ref_year, km
    Cartesian vel;    //!< ITRF velocity, km/year
    double ref_year = 2015.0;
    double latitude = 0.0;  //!< geodetic, radians
    double longitude = 0.0; //!< radians
    double height = 0.0;    //!< above the ellipsoid, km
};

//! Geodetic latitude, longitude and height from an ITRF position, by Bowring's
//! closed form. WGS84, which for this purpose is the same as the ITRF ellipsoid.
void geodetic(const Cartesian &p, double &lat, double &lon, double &height)
{
    constexpr double a = 6378.137;
    constexpr double f = 1.0 / 298.257223563;
    const double b = a * (1.0 - f);
    const double e2 = f * (2.0 - f);
    const double ep2 = e2 / (1.0 - e2);

    const double r = std::sqrt(p.x * p.x + p.y * p.y);
    const double theta = std::atan2(p.z * a, r * b);
    lat = std::atan2(p.z + ep2 * b * std::pow(std::sin(theta), 3),
                     r - e2 * a * std::pow(std::cos(theta), 3));
    lon = std::atan2(p.y, p.x);
    const double N = a / std::sqrt(1.0 - e2 * std::sin(lat) * std::sin(lat));
    height = r / std::cos(lat) - N;
}

/*! Tropospheric range delay for an optical wavelength, Marini and Murray
 *  (1973), which is the model the ILRS specifies for laser ranging. Returns
 *  the one-way delay in kilometres.
 *
 *  It is about 2.3 m at the zenith and 7 m at 20 degrees elevation, so leaving
 *  it out does not look like an error - it looks like a slightly wrong orbit.
 */
double marini_murray(double P, double T, double RH, double lambda, double lat,
                     double height_km, double elevation)
{
    const double e0 = RH / 100.0 * 6.11 *
                      std::pow(10.0, 7.5 * (T - 273.15) / (237.3 + T - 273.15));
    const double K = 1.163 - 0.00968 * std::cos(2.0 * lat) - 0.00104 * T +
                     0.00001435 * P;
    const double A = 0.002357 * P + 0.000141 * e0;
    const double B = 1.084E-8 * P * T * K +
                     4.734E-8 * P * P * 2.0 / (T * (3.0 - 1.0 / K));
    const double f_lambda = 0.9650 + 0.0164 / (lambda * lambda) +
                            0.000228 / std::pow(lambda, 4);
    const double f_phi =
        1.0 - 0.0026 * std::cos(2.0 * lat) - 0.00031 * height_km;
    const double sinE = std::sin(elevation);

    return 0.001 * (f_lambda / f_phi) * (A + B) /
           (sinE + B / (A + B) / (sinE + 0.01));
}

//! The inverse of geodetic(): an ITRF position, in km, from geodetic latitude
//! and longitude in radians and height above the ellipsoid in km. Optical
//! observers report where they are this way; laser stations are published as
//! Cartesian coordinates, so both directions are needed.
inline Cartesian ecef_from_geodetic(double lat, double lon, double height)
{
    constexpr double a = 6378.137;
    constexpr double f = 1.0 / 298.257223563;
    const double e2 = f * (2.0 - f);
    const double N = a / std::sqrt(1.0 - e2 * std::sin(lat) * std::sin(lat));
    return Cartesian((N + height) * std::cos(lat) * std::cos(lon),
                     (N + height) * std::cos(lat) * std::sin(lon),
                     (N * (1.0 - e2) + height) * std::sin(lat));
}

/*! Read observing sites given the way optical observers state them - degrees of
 *  latitude and longitude and metres of altitude - rather than as the Cartesian
 *  coordinates a laser station is published with.
 *
 *  These positions are good to tens of metres at best, which is a fraction of
 *  an arcsecond at a thousand kilometres and so below what visual astrometry
 *  can see. No plate motion: it would be centimetres.
 */
inline bool read_sites(const std::string &path,
                       std::map<std::string, Station> &out)
{
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '/') {
            continue;
        }
        std::istringstream ss(line);
        Station s;
        std::string name;
        double lat_deg, lon_deg, height_m;
        if (!(ss >> s.id >> name >> lat_deg >> lon_deg >> height_m)) {
            continue;
        }
        constexpr double deg = 3.14159265358979323846 / 180.0;
        s.latitude = lat_deg * deg;
        s.longitude = lon_deg * deg;
        s.height = height_m * 0.001;
        s.pos = ecef_from_geodetic(s.latitude, s.longitude, s.height);
        s.vel = Cartesian(0.0, 0.0, 0.0);
        s.ref_year = 2000.0;
        out[s.id] = s;
    }
    return !out.empty();
}

bool read_stations(const std::string &path, std::map<std::string, Station> &out)
{
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '/') {
            continue;
        }
        std::istringstream ss(line);
        Station s;
        std::string name;
        double x, y, z, vx, vy, vz;
        if (!(ss >> s.id >> name >> x >> y >> z >> vx >> vy >> vz >>
              s.ref_year)) {
            continue;
        }
        // The reference frame files are in metres; everything here is km.
        s.pos = Cartesian(x * 0.001, y * 0.001, z * 0.001);
        s.vel = Cartesian(vx * 0.001, vy * 0.001, vz * 0.001);
        geodetic(s.pos, s.latitude, s.longitude, s.height);
        out[s.id] = s;
    }
    return !out.empty();
}

using Store = std::vector<std::tuple<State_vector, Matrix6x6, Matrix6x5>>;

/*! Solve the normal equations after scaling each parameter by its own
 *  diagonal, and report the condition number of the scaled matrix.
 *
 *  Without this the solve is hopeless here, and not because the geometry is
 *  bad. A range moves by about one metre per metre of initial position, by a
 *  quarter of a million metres per metre per second of initial velocity, and
 *  by a billion metres per unit of radiation pressure scale over a three day
 *  arc. Those columns differ by twelve orders of magnitude, the normal matrix
 *  spans twenty-four, and a double precision inverse of it is noise - which
 *  presents as a condition number of 1e15 and a solver that cannot find a step,
 *  i.e. exactly as if the parameters were not observable.
 */
Eigen::VectorXd scaled_solve(const Eigen::MatrixXd &AtA,
                             const Eigen::VectorXd &AtL, double &condition)
{
    const int n = static_cast<int>(AtA.rows());
    Eigen::VectorXd d(n);
    for (int k = 0; k < n; ++k) {
        const double a = std::abs(AtA(k, k));
        d(k) = (a > 0.0) ? 1.0 / std::sqrt(a) : 1.0;
    }
    const Eigen::MatrixXd As = d.asDiagonal() * AtA * d.asDiagonal();
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(
        As, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd sv = svd.singularValues();
    condition = sv(0) / sv(sv.size() - 1);
    const Eigen::VectorXd y = svd.solve(d.asDiagonal() * AtL);
    return d.asDiagonal() * y;
}

/*! The propagator advances on a fixed grid; laser passes do not. Interpolate
 *  the stored trajectory to an arbitrary epoch: cubic Hermite on position and
 *  velocity, which at a ten second grid is accurate to a fraction of a
 *  millimetre, and linear on the partials, which only shape the normal
 *  equations.
 */
bool interpolate(const Store &store, double dt, double grid, State_vector &state,
                 Matrix6x6 &Phi, Matrix6x5 &Sens)
{
    const double u = dt / grid;
    const long int i = static_cast<long int>(std::floor(u));
    if (i < 0 || static_cast<size_t>(i) + 1 >= store.size()) {
        return false;
    }
    const size_t k = static_cast<size_t>(i);
    const double s = u - static_cast<double>(i);

    const State_vector &a = std::get<0>(store[k]);
    const State_vector &b = std::get<0>(store[k + 1]);

    const double h00 = 2 * s * s * s - 3 * s * s + 1;
    const double h10 = s * s * s - 2 * s * s + s;
    const double h01 = -2 * s * s * s + 3 * s * s;
    const double h11 = s * s * s - s * s;
    // Derivatives of the basis functions, for the interpolated velocity.
    const double g00 = 6 * s * s - 6 * s;
    const double g10 = 3 * s * s - 4 * s + 1;
    const double g01 = -6 * s * s + 6 * s;
    const double g11 = 3 * s * s - 2 * s;

    const long double px[2] = {a.x, b.x}, py[2] = {a.y, b.y},
                      pz[2] = {a.z, b.z};
    const long double vx[2] = {a.u, b.u}, vy[2] = {a.v, b.v},
                      vz[2] = {a.w, b.w};

    state = State_vector(
        h00 * px[0] + h10 * grid * vx[0] + h01 * px[1] + h11 * grid * vx[1],
        h00 * py[0] + h10 * grid * vy[0] + h01 * py[1] + h11 * grid * vy[1],
        h00 * pz[0] + h10 * grid * vz[0] + h01 * pz[1] + h11 * grid * vz[1],
        (g00 * px[0] + g01 * px[1]) / grid + g10 * vx[0] + g11 * vx[1],
        (g00 * py[0] + g01 * py[1]) / grid + g10 * vy[0] + g11 * vy[1],
        (g00 * pz[0] + g01 * pz[1]) / grid + g10 * vz[0] + g11 * vz[1],
        a.epoch + (s * grid));

    Phi = (1.0 - s) * std::get<1>(store[k]) + s * std::get<1>(store[k + 1]);
    Sens = (1.0 - s) * std::get<2>(store[k]) + s * std::get<2>(store[k + 1]);
    return true;
}

} // namespace sgnlTracking

#endif // SGNL_TRACKING_FIT_H
