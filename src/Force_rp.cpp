/*! @file Force_rp.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Force_rp base class.
 */

#include "../include/Force_rp.h"

void Force_rp::setup(const Resident_constants &rso_const,
                     std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    /******************* Setup for compute_panel_accel() *******************/

    double mc = sgnlOPS::c * 1000.0 * rso_const.mass;

    double area_nu, mu;

    double yoke_spec_f = 0.0;
    double yoke_spec_n = 0.0;
    double yoke_diff = 0.0;

    for (const auto &material : rso_const.solar_array.yoke) {
        area_nu = material.area * material.refl;
        mu = material.spec;

        yoke_spec_f += material.area - area_nu * mu;
        yoke_spec_n += area_nu * mu;
        yoke_diff += area_nu * (1.0 - mu);
    }

    panel_spec_f[0] = yoke_spec_f;
    panel_spec_n[0] = yoke_spec_n;
    panel_diff[0] = yoke_diff;

    panel_spec_f[1] = yoke_spec_f;
    panel_spec_n[1] = yoke_spec_n;
    panel_diff[1] = yoke_diff;

    for (const auto &material : rso_const.solar_array.front) {
        area_nu = material.area * material.refl;
        mu = material.spec;

        panel_spec_f[0] += material.area - area_nu * mu;
        panel_spec_n[0] += area_nu * mu;
        panel_diff[0] += area_nu * (1.0 - mu);
    }

    for (const auto &material : rso_const.solar_array.rear) {
        area_nu = material.area * material.refl;
        mu = material.spec;

        panel_spec_f[1] += material.area - area_nu * mu;
        panel_spec_n[1] += area_nu * mu;
        panel_diff[1] += area_nu * (1.0 - mu);
    }

    panel_spec_f[0] /= mc;
    panel_spec_n[0] /= -0.5 * mc;
    panel_diff[0] /= -1.5 * mc;

    panel_spec_f[1] /= mc;
    panel_spec_n[1] /= -0.5 * mc;
    panel_diff[1] /= -1.5 * mc;

    panel_spec_f[1] = -panel_spec_f[1];
    panel_spec_n[1] = -panel_spec_n[1];

    /******************* Setup Flux models *******************/

    // Enable calculation of solar properties in Resident_variables class
    state->enable_solar_properties();

    flux_models = Flux::get_flux_models(rso_const.solar_flux_model,
                                        rso_const.earth_flux_model);

    size_t eci_flux_num = 0;
    size_t ecef_flux_num = 0;

    for (auto &flux_model : flux_models) {
        flux_model->setup(state);
        flux_model->add_flux_num(eci_flux_num, ecef_flux_num);
    }

    // Reserve memory for vectors based on flux model contributions
    state->eci_fluxes.reserve(eci_flux_num);
    state->ecef_fluxes.reserve(ecef_flux_num);

    if (flux_models.size() == 0u) {
        std::stringstream error;
        error << "Force_rp: no flux models are present, all radiation pressure "
                 "forces will be 0.";
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_rp: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());

        panel_spec_f[0] = 0.0; // Prevent future NaN's
        panel_spec_n[0] = 0.0;
        panel_diff[0] = 0.0;

        panel_spec_f[1] = 0.0;
        panel_spec_n[1] = 0.0;
        panel_diff[1] = 0.0;
    }
}

void Force_rp::compute_acceleration()
{
    state->eci_fluxes.clear();
    state->ecef_fluxes.clear();

    for (auto &flux_model : flux_models) {
        flux_model->compute_flux();
    }
}

/*!
 * @fn compute_panel_accel(const std::vector<Fluxstruct> &fluxes,
                           const Attitude_state &attitude) const
 * @date 20 May 2016
 * @brief Computes the solar array's contribution to radiation pressure accel.
 *
 * This method also populates solar_array.flux values for the front and back of
 * the panels, so TRR force should only be evaluated AFTER RP has run.
 *
 * Based on Ziebart, M, "Generalised Analytical Solar Radiation
 * Pressure Modelling Algorithm for Spacecraft of Complex Shape",
 * Journal of Spacecraft and Rockets, Vol 41, No 5, pp 840-848, 2004.
 */
Cartesian Force_rp::compute_panel_accel(const std::vector<Fluxstruct> &fluxes,
                                        Cartesian panel_norm) const
{
    double cos_theta, flux_mag, flux_mag_sw, flux_mag_lw;
    double spec_f, spec_n, diff;

    Cartesian a(0.0, 0.0, 0.0);

    for (const auto &flux : fluxes) {
        cos_theta = -dot_product(panel_norm, flux.dir);

        // Correct flux magnitude for angle of incidence
        flux_mag_sw = cos_theta * flux.mag_sw;
        flux_mag_lw = cos_theta * flux.mag_lw;
        flux_mag = flux_mag_sw + flux_mag_lw;

        // Choose properties based on which side is illuminated
        if (cos_theta > 0) {
            spec_f = panel_spec_f[0];
            spec_n = panel_spec_n[0];
            diff = panel_diff[0];
            state->panel_flux_sw[0] += flux_mag_sw;
            state->panel_flux_lw[0] += flux_mag_lw;
        } else {
            spec_f = panel_spec_f[1];
            spec_n = panel_spec_n[1];
            diff = panel_diff[1];
            state->panel_flux_sw[1] -= flux_mag_sw;
            state->panel_flux_lw[1] -= flux_mag_lw;
        }

        a += (flux_mag * spec_f) * flux.dir +
             (flux_mag * (spec_n * cos_theta + diff)) * panel_norm;
        // Should have coefficients for visiable and infrared:
        // spec_f_vis, spec_f_ir, spec_n_is, spec_n_ir, diff_vis, diff_ir
    }

    return a;
}

void Force_rp::test_panel_accel(double lat_deg, double lon_deg) const
{
    Cartesian test_panel(0.0, 0.0, 1.0);
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

    Cartesian accel = compute_panel_accel(test_flux_vec, test_panel);

    accel *= 1000.0;

    accel.print();
    std::cout << accel.length() << " m/s^2" << std::endl;
}

std::unique_ptr<Force> Force_rp::get_rp_model(int model_num)
{
    std::unique_ptr<Force> rp_model;

    if (model_num == 2) {
        rp_model = std::make_unique<Force_rp_gridfile>();
    } else if (model_num == 3) {
        rp_model = std::make_unique<Force_rp_box_wing>();
    } else {
        rp_model = std::make_unique<Force_rp_analytic>();
    }

    return rp_model;
}
