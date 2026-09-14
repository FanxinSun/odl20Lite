/*! @file Force_earth_tide.cpp
	@author Santosh Bhattarai
	@date 22 June 2017
	@brief SGNL OPS implementation of solid earth tide.
 */

#include "../include/Force_earth_tide.h"

void Force_earth_tide::setup(const Resident_constants &rso_const,
                             std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    // We don't use this parameter, so this stops the compiler warning about it.
    sgnlOPS::ignore(rso_const);

    /***************************************
    The UCL numbering convention is:
    0 = Sun         5 = Jupiter
    1 = Mercury     6 = Saturn
    2 = Venus       7 = Uranus
    3 = Moon        8 = Neptune
    4 = Mars        9 = Pluto
    THIS SHOULD BE ADHERED TO WHEN RETRIEVING
    POSITION INFO FROM THE DATA STRUCTURE
    *****************************************/

    std::vector<bool> bodies_on(4); //!< Which solar syetem bodies are enabled

    // Array of solar system bodies to compute earth tide for
    bodies_on[0] = true;  // Sun
    bodies_on[1] = false; // Mercury
    bodies_on[2] = false; // Venus
    bodies_on[3] = true;  // Moon

    // Note that if Sun is not enabled here, it will be enabled elsewhere if SRP
    // is enabled, and therefore third body gravity will be calculated for it.
    state->ephem.enable_bodies(bodies_on);
}

void Force_earth_tide::compute_partial_derivatives() const
{
    constexpr double k2 = 0.2977;   // k2 Love number, elastic Earth
    constexpr double a = 6378.1363; // Earth semi-major axis, km
    constexpr double a5 = (a * a) * (a * a) * a;

    const Cartesian r_hat = state->eci_rso_hat;
    const Matrix3x3 rrt = col_times_row(r_hat, r_hat);

    Matrix3x3 dadr = Matrix3x3::Zero();

    for (const auto &body : state->ephem.bodies) {
        const Cartesian R = body.pos;
        const double R2 = R.length2();

        const double dot_prod = dot_product(r_hat, R);
        const double GM_R5 = body.GM / (R2 * R2 * std::sqrt(R2));

        double coef1 = (R2 - 5.0 * dot_prod * dot_prod) * GM_R5;
        double coef2 = (7.0 * dot_prod * dot_prod - 5.0) * GM_R5;
        double coef3 = 2.0 * GM_R5;
        double coef4 = -10.0 * dot_prod * GM_R5;

        Matrix3x3 coef_rRt = col_times_row(coef4 * r_hat, R);

        dadr.noalias() += diagonal(coef1) + coef2 * rrt +
                          col_times_row(coef3 * R, R) +
                          (coef_rRt + coef_rRt.transpose());
    }

    dadr *= 1.5 * k2 * a5 / (state->r2 * state->r2 * state->r);

    state->dadr_eci.noalias() += dadr;
}

void Force_earth_tide::compute_acceleration()
{
    constexpr double k2 = 0.2977;   // k2 Love number, elastic Earth
    constexpr double a = 6378.1363; // Earth semi-major axis, km
    constexpr double a5 = (a * a) * (a * a) * a;

    const Cartesian r_hat = state->eci_rso_hat;

    a_eci.set(0.0, 0.0, 0.0);

    for (const auto &body : state->ephem.bodies) {
        const Cartesian R = body.pos;
        const double R2 = R.length2();

        const double dot_prod = dot_product(r_hat, R);
        const double GM_R5 = body.GM / (R2 * R2 * std::sqrt(R2));

        a_eci += (2.0 * dot_prod * GM_R5) * R +
                 ((R2 - 5.0 * dot_prod * dot_prod) * GM_R5) * r_hat;
    }

    a_eci *= 1.5 * k2 * a5 / (state->r2 * state->r2);

    state->total_a_eci += a_eci;
}
