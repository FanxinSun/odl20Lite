/*! @file Ephemeris.cpp
	@author David Harrison
	@date 13 May 2016
	@brief SGNL OPS file defining the object Ephemeris, which serves as
		   an interface to the ephemeris functions in astrolib.cpp

    Originally based on code by Joe Heafner named sephem.
 */

#include "../include/Ephemeris.h"

#include "../external/fecsoft/astrolib.cpp"

Ephemeris::Ephemeris(const std::vector<bool> &solar_system_bodies)
{
    enable_bodies(solar_system_bodies);
}

// Custom swap friend function that doesn't touch Fecsoft
void ephem_swap(Ephemeris &lhs, Ephemeris &rhs)
{
    std::swap(lhs.r_earth, rhs.r_earth);
    std::swap(lhs.v_earth, rhs.v_earth);
    std::swap(lhs.enabled, rhs.enabled);

    std::swap(lhs.bodies, rhs.bodies);
    std::swap(lhs.helio_V_cross_R_term, rhs.helio_V_cross_R_term);
}

// Copy constructor also doesn't touch Fecsoft
Ephemeris::Ephemeris(const Ephemeris &in)
    : r_earth{in.r_earth}, v_earth{in.v_earth}, enabled{in.enabled},
      bodies{in.bodies}, helio_V_cross_R_term{in.helio_V_cross_R_term}
{
}

// Copy assignment operator
Ephemeris &Ephemeris::operator=(Ephemeris in)
{
    ephem_swap(*this, in);

    return *this;
}

// Move constructor
Ephemeris::Ephemeris(Ephemeris &&in)
    : Ephemeris() // initialize via default constructor
{
    ephem_swap(*this, in);
}

// This method enables planets as specified in the input array
// It cannot DISABLE any planets, once enabled they stay enabled
void Ephemeris::enable_bodies(const std::vector<bool> &solar_system_bodies)
{
    size_t lim = solar_system_bodies.size();
    if (lim > 10u) {
        lim = 10u;
    }

    // Looping through: Sun, 2 planets, Earth's moon, 6 remaining planets
    for (size_t i = 0; i < lim; ++i) {
        if (solar_system_bodies[i]) {
            enabled[i] = true;
        }
    }

    resize_output_vector();
}

void Ephemeris::resize_output_vector()
{
    // Values for GM in km^3/s^2 taken from DE430/DE431 on 2015-11-11:
    // http://ipnpr.jpl.nasa.gov/progress_report/42-196/196C.pdf
    // Planetary and Lunar Ephemerides DE430 and DE431, published 2014-02-15
    // Sun, Mercury, Venus, the Moon, Mars, Jupiter, Saturn, Uranus, Nept, Pluto
    // clang-format off
    // const double solar_system_GM[10] = { 132712440041.939400,     22031.780000,
    //       324858.592000,    4902.800066,        42828.375214, 126712764.800000,
    //     37940585.200000, 5794548.600000,      6836527.100580,       977.000000};

	// Values for GM in km^3/s^2 taken from DE405/DE406 on 2015-11-12:
	// ftp://ssd.jpl.nasa.gov/pub/eph/planets/ioms/de405.iom.pdf
	// JPL Planetary and Lunar Ephemerides DE405 Memo, published 1998-08-26
    // Sun, Mercury, Venus, the Moon, Mars, Jupiter, Saturn, Uranus, Nept, Pluto
    const double solar_system_GM[10] = { 132712440017.987,     22032.080,
                324858.599,    4902.801,        42828.314, 126712767.863,
              37940626.063, 5794549.007,      6836534.064,       981.601};
    // clang-format on

    size_t count = 0;
    for (size_t i = 0; i < 10; ++i) {
        if (enabled[i]) {
            count++;
        }
    }

    bodies.clear();
    bodies.reserve(count);

    for (size_t i = 0; i < 10; ++i) {
        if (enabled[i]) {
            Planetstruct body;
            body.GM = solar_system_GM[i];
            bodies.push_back(body);
        }
    }
}

double Ephemeris::converge_TDB_TT(Cartesian eci_rso, Timetag epoch)
{
    double rel_term, rel_term_old;
    double JD_TDB[2];        // TDB Julian date
    const double tol = 1E-9; // 1 ns tolerance

    Timestruct TT_tag = epoch.get_TT_tag();
    double TDB_minus_TT_approx = epoch.get_TDB_minus_TT();

    // Use current spacecraft position with previous v_earth to get estimate of
    // the relativistic correction term for TDB - TT
    rel_term =
        Timetag::get_TDB_minus_TT_rel_term(dot_product(eci_rso, v_earth));

    double rrd[6] = {};

    // Iterate until relativistic correction term converges to within tolerance
    do {

        JD_TDB[0] = static_cast<double>(2400001 + TT_tag.MJDN);
        JD_TDB[1] = (static_cast<double>(TT_tag.SOD - 43200) +
                     (TT_tag.SOD_frac + (TDB_minus_TT_approx + rel_term))) /
                    86400.0;

        if (JD_TDB[1] < 0.0) {
            JD_TDB[0] -= 1.0;
            JD_TDB[1] += 1.0;
        }

        /* Get Earth state vector from JPL ephemeris */
        fecsoft.pleph(JD_TDB, 3, 12, 2, rrd);

        // Update v_earth, Solar-system barycentric velocity of Earth
        v_earth.set(rrd[3], rrd[4], rrd[5]);

        rel_term_old = rel_term;

        rel_term =
            Timetag::get_TDB_minus_TT_rel_term(dot_product(eci_rso, v_earth));

    } while (std::fabs(rel_term - rel_term_old) > tol);

    // Update r_earth, Solar-system barycentric position of Earth
    r_earth.set(rrd[0], rrd[1], rrd[2]);

    return TDB_minus_TT_approx + rel_term;
}

double Ephemeris::compute_ephemeris(Cartesian eci_rso, Timetag epoch)
{
    double TDB_minus_TT = converge_TDB_TT(eci_rso, epoch);

    Timestruct TDB_tag = epoch.get_TT_tag();
    TDB_tag.SOD_frac += TDB_minus_TT;

    double JD_TDB[2]; // TDB Julian date
    JD_TDB[0] = static_cast<double>(2400001 + TDB_tag.MJDN);
    JD_TDB[1] =
        (static_cast<double>(TDB_tag.SOD - 43200) + TDB_tag.SOD_frac) / 86400.0;

    if (JD_TDB[1] < 0.0) {
        JD_TDB[0] -= 1.0;
        JD_TDB[1] += 1.0;
    }

    // Correct Targ for numbering used in original ephemeris code
    const int Targ[10] = {11, 1, 2, 10, 4, 5, 6, 7, 8, 9};

    double rrd[6] = {};
    Cartesian helio_v;

    size_t n = 0;
    for (size_t i = 0; i < 10; ++i) {
        if (enabled[i]) {

            /* Get body state vector from JPL ephemeris */
            if (i == 0) {
                // Interpolate for sun velocity in barycentre frame too
                fecsoft.pleph(JD_TDB, Targ[i], 12, 2, rrd);

                // Barycentric Earth velocity minus barycentric Sun velocity
                helio_v = v_earth - Cartesian(rrd[3], rrd[4], rrd[5]);
            } else {
                fecsoft.pleph(JD_TDB, Targ[i], 12, 1, rrd);
            }

            bodies[n].pos.set(rrd[0], rrd[1], rrd[2]);

            bodies[n].pos -= r_earth; // Convert to ECI frame

            // Calculate position of sun with aberration for SRP/ERP
            // if (i == 0) {
            //     solar_pos = bodies[0].pos;
            // }
            // Calculate position of moon with aberration for lunar eclipse
            // else if (i == 3) {
            //     lunar_pos = pos[3].pos;
            // }

            ++n;
        }
    }

    if (enabled[0]) {
        double R2 = bodies[0].pos.length2();

        double coef = std::round(3000000.0 * bodies[0].GM) /
                      (sgnlOPS::c2 * R2 * std::sqrt(R2));

        helio_V_cross_R_term = coef * cross_product(helio_v, bodies[0].pos);
    }

    return TDB_minus_TT;
}
