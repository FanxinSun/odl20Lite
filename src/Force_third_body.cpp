/*! @file Force_third_body.cpp
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS file defining the acceleration due to gravitational effects
		   of the sun, moon and solar system planets.
 */

#include "../include/Force_third_body.h"

void Force_third_body::setup(const Resident_constants &rso_const,
                             std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    // We don't use this parameter, so this stops the compiler warning about it.
    sgnlOPS::ignore(rso_const);

    /***************************************
	The UCL numbering convention is:
	0 = Sun			5 = Jupiter
	1 = Mercury		6 = Saturn
	2 = Venus       7 = Uranus
	3 = Moon        8 = Neptune
	4 = Mars        9 = Pluto
	THIS SHOULD BE ADHERED TO WHEN RETRIEVING
	POSITION INFO FROM THE DATA STRUCTURE
	*****************************************/

    std::vector<bool> bodies_on(10); //!< Which solar syetem bodies are enabled

    // Array of solar system bodies to compute third body gravity for
    bodies_on[0] = true;  // Sun
    bodies_on[1] = false; // Mercury
    bodies_on[2] = true;  // Venus
    bodies_on[3] = true;  // Moon
    bodies_on[4] = false; // Mars
    bodies_on[5] = true;  // Jupiter
    bodies_on[6] = false; // Saturn
    bodies_on[7] = false; // Uranus
    bodies_on[8] = false; // Neptune
    bodies_on[9] = false; // Pluto

    // Note that if Sun is not enabled here, it will be enabled elsewhere if SRP
    // is enabled, and therefore third body gravity will be calculated for it.
    state->ephem.enable_bodies(bodies_on);
}

/*
 * Ref: Montenbruck, Satellite Orbits, Page 248
 */
void Force_third_body::compute_partial_derivatives() const
{
    Cartesian r;
    double r2, xx, yy, zz, coef;
    Matrix3x3 dadr = Matrix3x3::Zero();

    for (const auto &body : state->ephem.bodies) {
        r = body.pos - state->eci_rso;
        xx = r.x * r.x;
        yy = r.y * r.y;
        zz = r.z * r.z;
        r2 = xx + yy + zz;

        coef = body.GM / (r2 * r2 * std::sqrt(r2));

        dadr(0, 0) += coef * (2.0 * xx - (yy + zz));
        dadr(1, 1) += coef * (2.0 * yy - (xx + zz));
        dadr(2, 2) += coef * (2.0 * zz - (xx + yy));

        coef *= 3.0;

        dadr(0, 1) += coef * (r.x * r.y);
        dadr(0, 2) += coef * (r.x * r.z);
        dadr(1, 2) += coef * (r.y * r.z);
    }

    dadr(1, 0) = dadr(0, 1);
    dadr(2, 0) = dadr(0, 2);
    dadr(2, 1) = dadr(1, 2);

    state->dadr_eci.noalias() += dadr;
}

void Force_third_body::compute_acceleration()
{
    double r_sc;  // distance of third body from spacecraft squared
    double r_geo; // distance of third body from geocentre squared

    double GMr_sc;  // GM over r^3 for spacecraft
    double GMr_geo; // GM over r^3 for geocentre

    Cartesian r, R;

    a_eci.set(0.0, 0.0, 0.0);

    for (const auto &body : state->ephem.bodies) {
        R = body.pos;
        r = R - state->eci_rso;

        r_sc = r.length2();
        r_geo = R.length2();

        GMr_sc = body.GM / (r_sc * std::sqrt(r_sc));
        GMr_geo = body.GM / (r_geo * std::sqrt(r_geo));

        a_eci += GMr_sc * r - GMr_geo * R;
    }

    state->total_a_eci += a_eci;
}
