/*! @file Force_rp_box_wing.cpp
	@author David Harrison
	@date 21 May 2016
	@brief SGNL OPS implementation of a box and wing radiation pressure model.
 */

#include "../include/Force_rp_box_wing.h"

void Force_rp_box_wing::setup(const Resident_constants &rso_const,
                              std::shared_ptr<Resident_variables> in_state)
{
    Force_rp::setup(rso_const, in_state);

    spec_f_pos = rso_const.face_area;
    spec_n_pos = rso_const.face_area;
    diff_pos = rso_const.face_area;

    spec_f_neg = rso_const.face_area;
    spec_n_neg = rso_const.face_area;
    diff_neg = rso_const.face_area;

    double mc = sgnlOPS::c * 1000.0 * rso_const.mass;

    spec_f_pos.x *= (1.0 - rso_const.nu_pos.x * rso_const.mu_pos.x) / mc;
    spec_n_pos.x *= -2.0 * rso_const.nu_pos.x * rso_const.mu_pos.x / mc;
    diff_pos.x *= rso_const.nu_pos.x * (1.0 - rso_const.mu_pos.x) / (-1.5 * mc);

    spec_f_pos.y *= (1.0 - rso_const.nu_pos.y * rso_const.mu_pos.y) / mc;
    spec_n_pos.y *= -2.0 * rso_const.nu_pos.y * rso_const.mu_pos.y / mc;
    diff_pos.y *= rso_const.nu_pos.y * (1.0 - rso_const.mu_pos.y) / (-1.5 * mc);

    spec_f_pos.z *= (1.0 - rso_const.nu_pos.z * rso_const.mu_pos.z) / mc;
    spec_n_pos.z *= -2.0 * rso_const.nu_pos.z * rso_const.mu_pos.z / mc;
    diff_pos.z *= rso_const.nu_pos.z * (1.0 - rso_const.mu_pos.z) / (-1.5 * mc);

    spec_f_neg.x *= (1.0 - rso_const.nu_neg.x * rso_const.mu_neg.x) / mc;
    spec_n_neg.x *= -2.0 * rso_const.nu_neg.x * rso_const.mu_neg.x / mc;
    diff_neg.x *= rso_const.nu_neg.x * (1.0 - rso_const.mu_neg.x) / (-1.5 * mc);

    spec_f_neg.y *= (1.0 - rso_const.nu_neg.y * rso_const.mu_neg.y) / mc;
    spec_n_neg.y *= -2.0 * rso_const.nu_neg.y * rso_const.mu_neg.y / mc;
    diff_neg.y *= rso_const.nu_neg.y * (1.0 - rso_const.mu_neg.y) / (-1.5 * mc);

    spec_f_neg.z *= (1.0 - rso_const.nu_neg.z * rso_const.mu_neg.z) / mc;
    spec_n_neg.z *= -2.0 * rso_const.nu_neg.z * rso_const.mu_neg.z / mc;
    diff_neg.z *= rso_const.nu_neg.z * (1.0 - rso_const.mu_neg.z) / (-1.5 * mc);

    spec_f_neg = -spec_f_neg;
    spec_n_neg = -spec_n_neg;

    if (rso_const.mass <= 0.0) {
        // We don't need to warn as Force_rp::setup does this already
        spec_f_pos.set(0.0, 0.0, 0.0); // Prevent future NaN's
        spec_n_pos.set(0.0, 0.0, 0.0);
        diff_pos.set(0.0, 0.0, 0.0);
        spec_f_neg.set(0.0, 0.0, 0.0);
        spec_n_neg.set(0.0, 0.0, 0.0);
        diff_neg.set(0.0, 0.0, 0.0);
    }
}

void Force_rp_box_wing::compute_acceleration()
{
    Force_rp::compute_acceleration();

    a_eci = compute_panel_accel(state->eci_fluxes, state->eci_attitude.panel) +
            compute_box_accel(state->eci_fluxes, state->eci_attitude);

    a_ecef =
        compute_panel_accel(state->ecef_fluxes, state->ecef_attitude.panel) +
        compute_box_accel(state->ecef_fluxes, state->ecef_attitude);

    state->total_a_eci += a_eci;
    state->total_a_ecef += a_ecef;
}

/*!
 * @fn compute_box_accel(const std::vector<Fluxstruct> &fluxes,
 *                       const Attitude_state &attitude) const)
 * @date 21 May 2016
 * @brief Computes a cuboids contribution to radiation pressure accel.
 *
 * This is a very similar function to compute_panel_accel in Force_rp.
 */
Cartesian
Force_rp_box_wing::compute_box_accel(const std::vector<Fluxstruct> &fluxes,
                                     const Attitude_state &att) const
{
    Cartesian spec_n, spec_f, diff, cos_theta;

    Cartesian a(0.0, 0.0, 0.0);

    for (const auto &flux : fluxes) {
        spec_n = spec_n_pos;
        spec_f = spec_f_pos;
        diff = diff_pos;

        cos_theta.x = -dot_product(att.x_hat, flux.dir);
        cos_theta.y = -dot_product(att.y_hat, flux.dir);
        cos_theta.z = -dot_product(att.z_hat, flux.dir);

        if (cos_theta.x < 0.0) {
            spec_n.x = spec_n_neg.x;
            spec_f.x = spec_f_neg.x;
            diff.x = diff_neg.x;
        }

        if (cos_theta.y < 0.0) {
            spec_n.y = spec_n_neg.y;
            spec_f.y = spec_f_neg.y;
            diff.y = diff_neg.y;
        }

        if (cos_theta.z < 0.0) {
            spec_n.z = spec_n_neg.z;
            spec_f.z = spec_f_neg.z;
            diff.z = diff_neg.z;
        }

        double mag = flux.mag_sw + flux.mag_lw;

        a += mag *
             (dot_product(cos_theta, spec_f) * flux.dir +
              (cos_theta.x * (spec_n.x * cos_theta.x + diff.x)) * att.x_hat +
              (cos_theta.y * (spec_n.y * cos_theta.y + diff.y)) * att.y_hat +
              (cos_theta.z * (spec_n.z * cos_theta.z + diff.z)) * att.z_hat);
    }

    return a;
}

void Force_rp_box_wing::test_box_accel(double lat_deg, double lon_deg) const
{
    Attitude_state test_att(Cartesian(1.0, 0.0, 0.0), Cartesian(0.0, 1.0, 0.0),
                            Cartesian(0.0, 0.0, 1.0), Cartesian(1.0, 0.0, 0.0));
    std::vector<Fluxstruct> test_flux_vec;
    Fluxstruct test_flux;

    double lat_rad = (90.0 - lat_deg) * sgnlOPS::D_DEG2RAD;
    double lon_rad = lon_deg * sgnlOPS::D_DEG2RAD;

    test_flux.mag_sw = 1368.0;
    test_flux.mag_lw = 0.0;

    test_flux.dir.x = -sin(lat_rad) * cos(lon_rad);
    test_flux.dir.y = -sin(lat_rad) * sin(lon_rad);
    test_flux.dir.z = -cos(lat_rad);

    test_flux_vec.push_back(test_flux);

    Cartesian accel = compute_box_accel(test_flux_vec, test_att);

    accel *= 1000.0;

    accel.print();
    std::cout << accel.length() << " m/s^2" << std::endl;
}
