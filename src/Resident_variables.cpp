/*! @file Resident_variables.cpp
	@author Santosh Bhattarai
	@date 10 November 2015
	@brief SGNL OPS file defining the object Resident_variables, which
           contains and updates all variables needed by the force models.
 */

#include "../include/Resident_variables.h"

void Resident_variables::reset()
{
    panel_flux_sw[0] = 0.0;
    panel_flux_sw[1] = 0.0;
    panel_flux_lw[0] = 0.0;
    panel_flux_lw[1] = 0.0;

    for (int i = 0; i < 5; ++i) {
        emp_partial[i].set(0.0, 0.0, 0.0);
    }
    srp_unscaled_eci.set(0.0, 0.0, 0.0);
    srp_unscaled_ecef.set(0.0, 0.0, 0.0);
    total_a_eci.set(0.0, 0.0, 0.0);
    total_a_ecef.set(0.0, 0.0, 0.0);

    dadr_eci << Matrix3x3::Zero();
    dadr_ecef << Matrix3x3::Zero();

    dadv_eci << Matrix3x3::Zero();
    dadv_ecef << Matrix3x3::Zero();
}

void Resident_variables::setup(const Configuration &config)
{
    frame_transform.setup(config.initial_state.epoch, config.simulation_time);
}

void Resident_variables::enable_solar_properties()
{
    need_solar_properties = true;

    ephem.enable_bodies(std::vector<bool>(1, true));
}

void Resident_variables::update(State_vector in_state)
{
    reset();

    eci = in_state;

    // Update positions of all enabled solar system bodies
    double TDB_minus_TT = ephem.compute_ephemeris(eci_rso, eci.epoch);

    // Rotate the ECI state vector into the ECEF frame
    pm = frame_transform.compute_rotations(eci.epoch, TDB_minus_TT);
    ecef = frame_transform.rotate_eci_to_ecef(eci);

    r2_ld = eci.x * eci.x + eci.y * eci.y + eci.z * eci.z;
    long double r_ld = std::sqrt(r2_ld);
    r2 = static_cast<double>(r2_ld);
    r = static_cast<double>(r_ld);

    // ECI Properties:

    long double eci_v2_ld = eci.u * eci.u + eci.v * eci.v + eci.w * eci.w;
    long double eci_v_ld = std::sqrt(eci_v2_ld);
    eci_v2 = static_cast<double>(eci_v2_ld);
    eci_v = static_cast<double>(eci_v_ld);

    // Get position and position unit vector in ECI as Cartesian object
    eci_rso.set(eci.x, eci.y, eci.z);
    eci_rso_hat.set(eci.x, eci.y, eci.z, r_ld);

    // Get velocity and velocity unit vector in ECI as Cartesian object
    eci_rso_vel.set(eci.u, eci.v, eci.v);
    // eci_rso_vel_hat.set(eci.u, eci.v, eci.w, eci_v_ld);

    eci_r_dot_v = static_cast<double>(eci.dot_product());
    eci_r_cross_v = eci.cross_product();

    // ECEF Properties:

    long double ecef_v2_ld =
        ecef.u * ecef.u + ecef.v * ecef.v + ecef.w * ecef.w;
    long double ecef_v_ld = std::sqrt(ecef_v2_ld);
    ecef_v2 = static_cast<double>(ecef_v2_ld);
    ecef_v = static_cast<double>(ecef_v_ld);

    // Get position and position unit vector in ECEF as Cartesian object
    ecef_rso.set(ecef.x, ecef.y, ecef.z);
    ecef_rso_hat.set(ecef.x, ecef.y, ecef.z, r_ld);

    // Get velocity and velocity unit vector in ECEF as Cartesian object
    ecef_rso_vel.set(ecef.u, ecef.v, ecef.v);
    // ecef_rso_vel_hat.set(ecef.u, ecef.v, ecef.w, ecef_v_ld);

    geodetic = ecef2geod();

    populate_VW_prime();

    if (need_solar_properties) {
        // Extract the postion of the sun from the ephemeris structure
        eci_sun = ephem.bodies[0].pos;
        eci_sun_distance = eci_sun.length();
        eci_sun_hat = eci_sun / eci_sun_distance;

        eci_rso_sun = eci_sun - eci_rso;           // Spacecraft-Sun vector
        rso_sun_distance2 = eci_rso_sun.length2(); // (Distance from sun)^2
        rso_sun_distance = std::sqrt(rso_sun_distance2);  // Distance from sun
        eci_rso_sun_hat = eci_rso_sun / rso_sun_distance; // Spacecraft-Sun dir

        ecef_sun = frame_transform.rotate_eci_to_ecef(eci_sun);
        ecef_sun_hat = normalise(ecef_sun);

        ecef_rso_sun = ecef_sun - ecef_rso;
        ecef_rso_sun_hat = ecef_rso_sun / rso_sun_distance;

        // Eclipse state determination
        eclipse_state = Eclipse_model::eclipse(
            rso_sun_distance, eci_sun_distance, ecef_rso, ecef_rso_sun);

        constexpr bool follow_IGS = false;
        eci_attitude.set_yaw_steering(eci_rso, eci_sun, follow_IGS);
        ecef_attitude.set_yaw_steering(ecef_rso, ecef_sun, follow_IGS);

        // computing the beta angle
        // beta_angle = eciSun.h/(|eciSun|*|h|)
        beta_angle = std::asin(dot_product(eci_sun, eci_r_cross_v) /
                               (eci_sun_distance * eci_r_cross_v.length()));

        // computing the Earth-spacecraft-Sun angle a.k.a. EPS angle
        // eps_angle = acos(pHat.zHat)
        eps_angle = std::acos(dot_product(eci_rso_sun_hat, eci_attitude.z_hat));

        // if (eclipse_state != 1)
        // {
        //     std::cout << "\n Eclipse state: " << eclipse_state
        //               << "\n Beta angle: " << beta_angle
        //               << "\n EPS angle: " << eps_angle
        //               << std::endl;
        // }

    } else {
        // We can at least set the z axis of the body frame without the sun
        // Useful for Antenna Thrust
        eci_attitude.z_hat = -eci_rso_hat;
        ecef_attitude.z_hat = -ecef_rso_hat;
    }
}

void Resident_variables::print()
{
    if (need_solar_properties) {
        std::cout << "\nSun position in ECI:";
        eci_sun.print();
        std::cout << "\nMagnitude of sun position in ECI: " << eci_sun_distance;
        std::cout << "\nEPS angle: " << eps_angle;
    }

    eci_attitude.print();
}

void Resident_variables::set_degree_order(size_t n, size_t m)
{
    n += 2;
    m += 2;

    m = (m > n) ? n : m;
    order = (m > order) ? m : order;

    if (n > degree) {
        degree = n;

        generate_VW_coefs();

        // For V' and W' functions
        VW.resize((degree + 1) * (degree + 2) / 2);
    }
}

void Resident_variables::set_earth_radius(double in_Re)
{
    long double scale = 1000000000.0L;
    Re = in_Re;
    Re_ld = std::round(static_cast<long double>(Re) * scale) / scale;
}

void Resident_variables::generate_VW_coefs()
{
    // For V' and W' function coefficients
    VWh.resize((degree + 1) * (degree + 2) / 2, 0.0);

    size_t n, m;
    double n_d, m_d;

    m_d = 0;
    for (m = 0; m <= degree; ++m) {

        n_d = m_d + 2.0;
        for (n = m + 2; n <= degree; ++n) {

            VWh[n * (n + 1) / 2 + m] = ((n_d + m_d - 1.0) * (n_d - m_d - 1.0)) /
                                       ((2.0 * n_d - 1.0) * (2.0 * n_d - 3.0));

            n_d += 1.0;
        }

        m_d += 1.0;
    }

} // End of function generate_VW_coefs

//! Calculate V' and W' for current position.
void Resident_variables::populate_VW_prime()
{
    size_t n, m;

    // 1D vector locations for (n,m), (n-1,m), (n-2,m), (m,m) and (m-1,m-1)
    size_t nm, n1m, n2m, mm, m1m1;

    long double Rer2 = Re_ld / r2_ld;
    double Re2r2 = static_cast<double>(Re_ld * Re_ld / r2_ld);

    double x, y, z;
    x = static_cast<double>(ecef.x * Rer2);
    y = static_cast<double>(ecef.y * Rer2);
    z = static_cast<double>(ecef.z * Rer2);

    VW[0].V = Re / r;
    VW[0].W = 0.0;

    VW[1].V = z * VW[0].V;
    VW[1].W = 0.0;

    nm = 1;
    n1m = 1;
    n2m = 0;

    // Compute the Vn0 terms (all Wn0 terms are 0)
    for (n = 2; n <= degree; ++n) {

        nm += n;
        VW[nm].V = z * VW[n1m].V - VWh[nm] * Re2r2 * VW[n2m].V;
        n2m = n1m;
        n1m = nm;
    }

    mm = 0;
    m1m1 = 0;

    // Compute the Vmm and Wmm terms
    for (m = 1; m <= order; ++m) {

        mm += m + 1;
        VW[mm].V = x * VW[m1m1].V - y * VW[m1m1].W;
        VW[mm].W = x * VW[m1m1].W + y * VW[m1m1].V;
        // V[m * (m + 1) / 2 + m] =
        //     x * V[(m - 1) * m / 2 + m - 1] - y * W[(m - 1) * m / 2 + m - 1];

        m1m1 = mm;

        n = m + 1;
        nm = mm + n;
        n1m = nm;
        n2m = mm;

        // Compute Vm+1,m and Wm+1,m terms
        if (n <= degree) {
            VW[nm].V = z * VW[n2m].V;
            VW[nm].W = z * VW[n2m].W;
            // V[n * (n + 1) / 2 + m] = z * V[(n - 1) * n / 2 + m];

            // Compute the Vnm and Wnm terms
            for (n = m + 2; n <= degree; ++n) {

                nm += n;

                VW[nm].V = z * VW[n1m].V - VWh[nm] * Re2r2 * VW[n2m].V;
                VW[nm].W = z * VW[n1m].W - VWh[nm] * Re2r2 * VW[n2m].W;
                // V[n * (n + 1) / 2 + m] =
                //     z * V[(n - 1) * n / 2 + m] -
                //     h[n * (n + 1) / 2 + m] * V[(n - 2) * (n - 1) / 2 + m];

                n2m = n1m;
                n1m = nm;
            }
        }
    }
} // end of function populate_VW_prime

// Takes in LLAstruct of lat/long in radians and altitude in km
// Returns Cartesian of ECEF position in km
// https://en.wikipedia.org/wiki/Geographic_coordinate_conversion
Cartesian Resident_variables::geod2ecef(LLAstruct coord)
{
    constexpr double a = sgnlOPS::wgs84_equatorial_radius;
    constexpr double e2 = sgnlOPS::wgs84_e2;

    Cartesian result;

    double N_denom = std::sqrt(1.0 - coord.slat * coord.slat * e2);

    result.x = (a / N_denom + coord.alt) * coord.clat;
    result.y = result.x * coord.slon;
    result.x *= coord.clon;

    result.z = (a * (1.0 - e2) / N_denom + coord.alt) * coord.slat;

    return result;
}

// Takes in Cartesian of ECEF position in km
// Returns LLAstruct of lat/long in radians and altitude in km
// https://en.wikipedia.org/wiki/Geographic_coordinate_conversion
LLAstruct Resident_variables::ecef2geod(long double x, long double y,
                                        long double z)
{
    constexpr double a = sgnlOPS::wgs84_equatorial_radius;
    constexpr double a2 = sgnlOPS::wgs84_equatorial_radius2;
    constexpr double b = sgnlOPS::wgs84_polar_radius;
    constexpr double b_a2 = sgnlOPS::wgs84_b_a2;
    constexpr double e2 = sgnlOPS::wgs84_e2;
    constexpr double e4 = e2 * e2;

    double z2 = static_cast<double>(z * z);
    long double w2_ld = x * x + y * y;
    long double w_ld = std::sqrt(w2_ld);
    double w2 = static_cast<double>(w2_ld);
    double w = static_cast<double>(w_ld);

    double F = 54.0 * b * b * z2;

    double G = w2 + (1.0 - e2) * z2 - a2 * e4;

    double C = e4 * F * w2 / (G * G * G);

    double S = std::cbrt(1.0 + C + std::sqrt(C * (C + 2.0)));
    double S2 = S * S;

    double P = (S2 + S + 1.0) * G;
    P = S2 * F / (P * P * 3.0);

    double Q = std::sqrt(1.0 + 2.0 * e4 * P);
    double Qp1 = Q + 1.0;

    double w0 =
        std::sqrt((0.5 * a2 * Qp1 * Qp1 - P * (1.0 - e2) * z2) / (Q * Qp1) -
                  0.5 * P * w2) -
        P * e2 * w / Qp1;

    double U = w - e2 * w0;
    U *= U;
    double V = std::sqrt(U + (1.0 - e2) * z2);
    U = std::sqrt(U + z2);

    LLAstruct result;

    long double lat_upper = static_cast<long double>(V + a * e2) * z;
    long double lat_lower = static_cast<long double>(V) * w_ld;
    long double mag = std::sqrt(lat_upper * lat_upper + lat_lower * lat_lower);

    // We take the opportunity to set cos(lat) and others
    // because it is easier and more precise to do so here

    result.lat = static_cast<double>(std::atan(lat_upper / lat_lower));
    result.clat = static_cast<double>(lat_lower / mag);
    result.slat = static_cast<double>(lat_upper / mag);

    result.lon = static_cast<double>(std::atan2(y, x));
    result.clon = static_cast<double>(x / w_ld);
    result.slon = static_cast<double>(y / w_ld);

    result.alt = U * (1.0 - b_a2 * a / V);

    return result;
}
