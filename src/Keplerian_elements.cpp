/*! @file Keplerian_elements.cpp
	@author David Harrison
	@date 21 March 2016
	@brief SGNL OPS file defining the object Keplerian_elements.
 */

#include "../include/Keplerian_elements.h"

Keplerian_elements::Keplerian_elements()
{
    sma = 6600.0L; // ~220 km orbit
    ecc = 0.0L;
    inc = 0.0L;
    argp = 0.0L;
    raan = 0.0L;
    tran = 0.0L;
    ecan = 0.0L;
    mean = 0.0L;

    // Pull default gravity parameter from constants
    GM = static_cast<long double>(sgnlOPS::GM);

    update_state_vector();
}

Keplerian_elements::Keplerian_elements(long double in_sma, long double in_ecc,
                                       long double in_inc, long double in_argp,
                                       long double in_raan, long double in_tran,
                                       long double in_GM, Timetag in_epoch)
{
    set(in_sma, in_ecc, in_inc, in_argp, in_raan, in_tran, in_GM, in_epoch);
}

Keplerian_elements::Keplerian_elements(State_vector in_state, long double in_GM)
{
    GM = in_GM;

    set(in_state);
}

Keplerian_elements::Keplerian_elements(long double in_x, long double in_y,
                                       long double in_z, long double in_u,
                                       long double in_v, long double in_w,
                                       Timetag in_epoch, long double in_GM)
{
    GM = in_GM;

    set(in_x, in_y, in_z, in_u, in_v, in_w, in_epoch);
}

void Keplerian_elements::set(long double in_sma, long double in_ecc,
                             long double in_inc, long double in_argp,
                             long double in_raan, long double in_tran,
                             long double in_GM, Timetag in_epoch)
{
    sma = in_sma;
    ecc = in_ecc;
    inc = std::fmod(in_inc, sgnlOPS::LD_2_PI);
    argp = std::fmod(in_argp, sgnlOPS::LD_2_PI);
    raan = std::fmod(in_raan, sgnlOPS::LD_2_PI);
    tran = std::fmod(in_tran, sgnlOPS::LD_2_PI);

    GM = in_GM;

    epoch = in_epoch;

    update_ecan_from_tran();
    update_mean_from_ecan();

    update_state_vector();
}

void Keplerian_elements::set(State_vector in_state)
{
    x = in_state.x;
    y = in_state.y;
    z = in_state.z;
    u = in_state.u;
    v = in_state.v;
    w = in_state.w;
    epoch = in_state.epoch;

    update_elements();
}

void Keplerian_elements::set(long double in_x, long double in_y,
                             long double in_z, long double in_u,
                             long double in_v, long double in_w,
                             Timetag in_epoch)
{
    State_vector::set(in_x, in_y, in_z, in_u, in_v, in_w, in_epoch);

    update_elements();
}

void Keplerian_elements::set_argp(long double in_argp, bool update_state_vec)
{
    argp = std::fmod(in_argp, sgnlOPS::LD_2_PI);

    if (update_state_vec) {
        update_state_vector();
    }
}

void Keplerian_elements::set_raan(long double in_raan, bool update_state_vec)
{
    raan = std::fmod(in_raan, sgnlOPS::LD_2_PI);

    if (update_state_vec) {
        update_state_vector();
    }
}

void Keplerian_elements::set_tran(long double in_tran)
{
    tran = std::fmod(in_tran, sgnlOPS::LD_2_PI);

    update_ecan_from_tran();
    update_mean_from_ecan();

    update_state_vector();
}

void Keplerian_elements::set_ecan(long double in_ecan)
{
    ecan = std::fmod(in_ecan, sgnlOPS::LD_2_PI);

    update_tran_from_ecan();
    update_mean_from_ecan();

    update_state_vector();
}

void Keplerian_elements::set_mean(long double in_mean)
{
    mean = std::fmod(in_mean, sgnlOPS::LD_2_PI);

    update_ecan_from_mean();
    update_tran_from_ecan();

    update_state_vector();
}

void Keplerian_elements::update_mean_from_ecan()
{
    mean = ecan - ecc * std::sin(ecan);
}

void Keplerian_elements::update_tran_from_ecan()
{
    long double sin_tran, cos_tran;

    // Not named correctly, these are both missing a division by (1 - ecc*cos(ecan))
    sin_tran = std::sqrt(1.0L - ecc * ecc) * std::sin(ecan);
    cos_tran = std::cos(ecan) - ecc;

    // But we don't do the division as it will cancel out in here anyway
    tran = std::atan2(sin_tran, cos_tran);

    if (tran < 0.0L) {
        tran += sgnlOPS::LD_2_PI; // Bring E into range 0 to 2PI
    }
}

void Keplerian_elements::update_ecan_from_tran()
{
    long double sin_ecan, cos_ecan;

    // Not named correctly, these are both missing a division by (1 + ecc*cos(tran))
    sin_ecan = std::sqrt(1.0L - ecc * ecc) * std::sin(tran);
    cos_ecan = std::cos(tran) + ecc;

    // But we don't do the division as it will cancel out in here anyway
    ecan = std::atan2(sin_ecan, cos_ecan);

    if (ecan < 0.0L) {
        ecan += sgnlOPS::LD_2_PI; // Bring E into range 0 to 2PI
    }
}

/****************************************************
Function: update_ecan_from_mean

  Calculates the eccentric anomaly in radians using
  Newton's root finding method or a fixed point
  iteration method depending on which one is faster

  MKZ November 2002
*****************************************************/
void Keplerian_elements::update_ecan_from_mean()
{
    long double E; // initial estimate for ecan

    constexpr long double tol = 0x1.0p-50L; // maximum difference allowed

    ecan = (ecc < 0.75L) ? mean : (ecc * sgnlOPS::LD_PI + mean) / (1.0L + ecc);

    // Calculate using Newton's method
    do {
        E = ecan;
        ecan = E - (E - ecc * std::sin(E) - mean) / (1.0L - ecc * std::cos(E));
    } while (std::abs(E - ecan) > tol);
}

/************************************************
* Function: update_state_vector (originally kep2cart)
* Updates the position and velocity elements
* of the inertial state vector from Keplerian
* elements
*	MKZ UCL, February 2003
*
************************************************/
void Keplerian_elements::update_state_vector()
{
    long double sqrt1me2 = std::sqrt(1.0L - ecc * ecc);

    long double cos_ecan = std::cos(ecan);
    long double sin_ecan = std::sin(ecan);

    // Compute the magnitude of the Gaussian vectors at the required point
    long double gaussX = sma * (cos_ecan - ecc);    // Magnitude of
    long double gaussY = sma * sqrt1me2 * sin_ecan; // Gaussian vectors

    // Using Montenbruck eq 2.44 pg 24, with the additional substitution that:
    // r = a(1 - e*cos(E))
    long double XYdotcommon = std::sqrt(GM / sma) / (1.0L - ecc * cos_ecan);

    long double gaussXdot = -sin_ecan * XYdotcommon;           // Gaussian vel.
    long double gaussYdot = cos_ecan * sqrt1me2 * XYdotcommon; // components

    long double cos_inc = std::cos(inc);
    long double sin_inc = std::sin(inc);

    long double cos_argp = std::cos(argp);
    long double cos_raan = std::cos(raan);

    long double sin_argp = std::sin(argp);
    long double sin_raan = std::sin(raan);

    long double cc = cos_argp * cos_raan;
    long double cs = cos_argp * sin_raan;
    long double sc = sin_argp * cos_raan;
    long double ss = sin_argp * sin_raan;

    long double P[3], Q[3]; // Components of the unit Gaussian vectors

    P[0] = cc - ss * cos_inc;
    P[1] = cs + sc * cos_inc;
    P[2] = sin_argp * sin_inc;

    Q[0] = -sc - cs * cos_inc;
    Q[1] = -ss + cc * cos_inc;
    Q[2] = cos_argp * sin_inc;

    x = gaussX * P[0] + gaussY * Q[0];
    y = gaussX * P[1] + gaussY * Q[1];
    z = gaussX * P[2] + gaussY * Q[2];

    u = gaussXdot * P[0] + gaussYdot * Q[0];
    v = gaussXdot * P[1] + gaussYdot * Q[1];
    w = gaussXdot * P[2] + gaussYdot * Q[2];
}

/**********************************************
Function: update_elements
Description: Cartesian to Keplerian elements
conversion routine. The cartesian state
vector has to be in Earth-Centred Inertial
coordinates. Note that this routine is designed
for the analysis of satellites in elliptical orbits
gravitationally bound to the Earth. If the
trajectory is parabolic or hyperbolic the
function will bomb

  MKZ UCL
**********************************************/
void Keplerian_elements::update_elements()
{
    long double e[3], h[3], n[2];
    // e = eccentricity or Lenz vector
    // h = specific angular momentum, independent of mass
    // n = node vector, 2D

    long double r, r2;            // position mag, position mag squared
    long double v2, rdotv;        // velocity mag squared, r dot v
    long double hx2hy2, e2;       // useful temporary variables
    long double coef;             // used when calculating eccentricity vector
    long double tol = 0x1.0p-70L; // tolerance when detecting near-circular
                                  // and near-equatorial orbits

    r2 = x * x + y * y + z * z;
    r = std::sqrt(r2);
    v2 = u * u + v * v + w * w;
    rdotv = dot_product();

    // Compute the semi-major axis
    sma = r * GM / (2.0L * GM - v2 * r);

    // Compute the eccentricity or Lenz vector multiplied by GM
    coef = v2 - GM / r;

    e[0] = coef * x - rdotv * u;
    e[1] = coef * y - rdotv * v;
    e[2] = coef * z - rdotv * w;

    e2 = e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    ecc = std::sqrt(e2 / (GM * GM));

    // Compute the specific angular momentum, r x v
    cross_product(h[0], h[1], h[2]);

    hx2hy2 = h[0] * h[0] + h[1] * h[1];

    // Compute the inclination
    inc = h[2] / std::sqrt(hx2hy2 + h[2] * h[2]);

    if (inc <= -1.0L) {
        inc = sgnlOPS::LD_PI;
    } else if (inc >= 1.0L) {
        inc = 0.0L;
    } else {
        inc = std::acos(inc);
    }

    // Compute the node vector, [0, 0, 1] x h
    n[0] = -h[1];
    n[1] = h[0];

    // std::cout << std::setprecision(19);
    // std::cout << e2 << ", " << hx2hy2 << ", " << tol << std::endl;
    // std::cout << (e2 > tol) << ", " << (hx2hy2 > tol) << std::endl;

    // Compute the true anomaly
    if (e2 > tol) {

        tran = (e[0] * x + e[1] * y + e[2] * z) / std::sqrt(e2 * r2);

        if (tran <= -1.0L) {
            tran = sgnlOPS::LD_PI;
        } else if (tran >= 1.0L) {
            tran = 0.0L;
        } else {
            tran = std::acos(tran);
            if (rdotv < 0.0L && tran > 0.0L) {
                tran = sgnlOPS::LD_2_PI - tran;
            }
        }

    } else {
        // If eccentricity is near zero, fix ths vector for argp calculation
        e[0] = x;
        e[1] = y;
        e[2] = z;
        e2 = r2;

        // Prevents NaN for circular orbits
        tran = 0.0L;
    }

    if (hx2hy2 > tol) {

        // Compute the argument of perigee
        argp = (n[0] * e[0] + n[1] * e[1]) / std::sqrt(hx2hy2 * e2);

        // Compute the right ascension of the ascending node
        raan = n[0] / std::sqrt(hx2hy2);

        if (raan <= -1.0L) {
            raan = sgnlOPS::LD_PI;
        } else if (raan >= 1.0L) {
            raan = 0.0L;
        } else {
            raan = std::acos(raan);
            if (n[1] < 0.0L && raan > 0.0L) {
                raan = sgnlOPS::LD_2_PI - raan;
            }
        }

    } else {
        // Prevents NaN for equatorial orbits
        // Set the node vector equal to [1, 0, 0], so that the argument of
        // perigee is at the current position
        argp = e[0] / std::sqrt(e2);

        raan = 0.0L; // Prevents NaN for exactly equatorial orbits
    }

    if (argp <= -1.0L) {
        argp = sgnlOPS::LD_PI;
    } else if (argp >= 1.0L) {
        argp = 0.0L;
    } else {
        argp = std::acos(argp);
        if (e[2] < 0.0L && argp > 0.0L) {
            argp = sgnlOPS::LD_2_PI - argp;
        }
    }

    update_ecan_from_tran();
    update_mean_from_ecan();
}

std::string State_vector::str() const //!< Generate representative string
{
    std::stringstream out;
    out.precision(19);

    out << "x = " << x << " km\n"
        << "y = " << y << " km\n"
        << "z = " << z << " km\n"
        << "u = " << u << " km/s\n"
        << "v = " << v << " km/s\n"
        << "w = " << w << " km/s\n"
        << epoch.str_UTC_datestamp();

    return out.str();
}

void State_vector::print() const //!< Print to screen (prints output of str())
{
    std::cout << str() << std::endl;
}

std::string Keplerian_elements::str() const
{
    return str_elements() + std::string("\n") + str_state_vector();
}

std::string Keplerian_elements::str_elements() const
{
    std::stringstream out;
    out.precision(19);

    out << "sma = " << sma << " km\n"
        << "ecc = " << ecc << "\n"
        << "inc = " << inc << " rad\n"
        << "argp = " << argp << " rad\n"
        << "raan = " << raan << " rad\n"
        << "tran = " << tran << " rad";

    return out.str();
}

std::string Keplerian_elements::str_state_vector() const
{
    return State_vector::str();
}

// Print Keplerian elements and state vector to screen
void Keplerian_elements::print() const
{
    std::cout << str() << std::endl;
}

// Print Keplerian elements to screen
void Keplerian_elements::print_elements() const
{
    std::cout << str_elements() << std::endl;
}

// Print state vector to screen
void Keplerian_elements::print_state_vector() const
{
    std::cout << str_state_vector() << std::endl;
}
