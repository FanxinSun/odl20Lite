/*! @file Force_gr_correction.cpp
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of general relativity corrections to orbits.
 */

#include "../include/Force_gr_correction.h"

void Force_gr_correction::setup(const Resident_constants &rso_const,
                                std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    // Enable calculation of solar properties in Resident_variables class
    state->enable_solar_properties();

    GM = rso_const.GM;

    // The factor of a million is to convert c^2 from m^2/s^2 into km^2/s^2.
    // But we can also round since GM is only given to 4 decimal places anyway.
    minus4GMc2 = -std::round(4000000.0 * GM) / sgnlOPS::c2;
}

void Force_gr_correction::compute_partial_derivatives() const
{
    // J_2 is the Earth's angular momentum per unit mass (in km^2/s), halved
    // The rotation axis for the Earth is the z axis in the ECEF frame
    const Cartesian J_2 =
        0.5 * 980.0 * Cartesian(state->frame_transform.R(2, 0),
                                state->frame_transform.R(2, 1),
                                state->frame_transform.R(2, 2));

    const Cartesian three_J_2 = 3.0 * J_2;

    const double m4GMc2r3 = minus4GMc2 / (state->r * state->r2);
    const double m4GMc2r4 = minus4GMc2 / (state->r2 * state->r2);

    const Cartesian r = state->eci_rso;
    const Cartesian rh = state->eci_rso_hat;
    const Cartesian v = state->eci_rso_vel;

    const double rTv = state->eci_r_dot_v;
    const Cartesian rhxv = cross_product(rh, v);

    const Matrix3x3 vrT = col_times_row(v, r);
    const double three_rhTJ_2 = dot_product(rh, three_J_2);
    const double rv2_4 = 0.25 * state->r * state->eci_v2;

    Matrix3x3 dadr = diagonal(GM - rv2_4) -
                     col_times_row(rh, (4.0 * GM - 3.0 * rv2_4) * rh) +
                     col_times_row(v, (state->r * v - 3.0 * rTv * rh));
    dadr.noalias() -=
        skew_symmetric(three_rhTJ_2 * v) +
        col_times_row(rhxv, (5.0 * three_rhTJ_2 * rh - three_J_2)) +
        col_times_row(cross_product(v, three_J_2), rh);
    dadr *= m4GMc2r4;

    Matrix3x3 dadv = diagonal(rTv) + skew_symmetric(three_rhTJ_2 * rh - J_2);
    dadv.noalias() += vrT - 0.5 * vrT.transpose();
    dadv = m4GMc2r3 * dadv + skew_symmetric(state->ephem.helio_V_cross_R_term);

    state->dadr_eci.noalias() += dadr;
    state->dadv_eci.noalias() += dadv;
}

void Force_gr_correction::compute_acceleration()
{
    // J_2 is the Earth's angular momentum per unit mass (in km^2/s), halved
    // The rotation axis for the Earth is the z axis in the ECEF frame
    const Cartesian J_2 =
        0.5 * 980.0 * Cartesian(state->frame_transform.R(2, 0),
                                state->frame_transform.R(2, 1),
                                state->frame_transform.R(2, 2));

    const double m4GMc2r4 = minus4GMc2 / (state->r2 * state->r2);

    const double r_coef = GM - 0.25 * state->r * state->eci_v2;
    const double v_coef = state->r * state->eci_r_dot_v;

    const double rcv_coef = 3.0 * dot_product(state->eci_rso_hat, J_2);
    const double vcJ_coef = state->r;

    a_eci =
        m4GMc2r4 * (r_coef * state->eci_rso + v_coef * state->eci_rso_vel -
                    rcv_coef * state->eci_r_cross_v -
                    vcJ_coef * cross_product(state->eci_rso_vel, J_2)) +
        cross_product(state->ephem.helio_V_cross_R_term, state->eci_rso_vel);

    // Cartesian a1 =
    //     minus4GMc2r4 * (r_coef * state->eci_rso + v_coef * state->eci_rso_vel);
    // Cartesian a2 =
    //     minus4GMc2r4 * (rcv_coef * state->eci_r_cross_v -
    //                     vcJ_coef * cross_product(state->eci_rso_vel, J_2));
    // Cartesian a3 =
    //     cross_product(state->ephem.helio_V_cross_R_term, state->eci_rso_vel);
    //
    // std::cout << "Accel due to Schwarzschild terms: " << std::endl;
    // a1.print();
    // std::cout << std::endl << "Accel due to frame dragging: " << std::endl;
    // a2.print();
    // std::cout << std::endl << "Accel due to solar precession: " << std::endl;
    // a3.print();
    // std::cout << std::endl << "Total GR correction: " << std::endl;
    // a_eci.print();
    // std::cout << std::endl << "----------------------------------" << std::endl;

    state->total_a_eci += a_eci;
}
