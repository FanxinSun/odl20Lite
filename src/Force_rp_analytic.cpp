/*! @file Force_rp_analytic.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of an analytic radiation pressure method.
 */

#include <cmath>
#include "../include/Force_rp_analytic.h"

void Force_rp_analytic::setup(const Resident_constants &rso_const,
                              std::shared_ptr<Resident_variables> in_state)
{
    Force_rp::setup(rso_const, in_state);

    state->srp_scale = rso_const.srp_scale;
    state->srp_scale_amp = rso_const.srp_scale_amp;
    state->srp_scale_period = rso_const.srp_scale_period;
    state->srp_scale_t0 = state->eci.epoch.get_MJD_UTC();

    a_coef = rso_const.area *
             (9.0 + 4.0 * rso_const.nu * (1.0 - rso_const.mu)) /
             (9000.0 * rso_const.mass * sgnlOPS::c);

    if (rso_const.area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_rp_analytic: spacecraft area is 0 or negative, "
              << rso_const.area << " m^2.";
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        // We don't need to warn as Force_rp::setup does this already
        a_coef = 0.0; // Prevent future NaN's
    }
}

void Force_rp_analytic::compute_acceleration()
{
    Force_rp::compute_acceleration();

    a_eci.set(0.0, 0.0, 0.0);
    a_ecef.set(0.0, 0.0, 0.0);

    for (const auto &eci_flux : state->eci_fluxes) {
        a_eci += eci_flux.dir * (a_coef * (eci_flux.mag_sw + eci_flux.mag_lw));
    }

    for (const auto &ecef_flux : state->ecef_fluxes) {
        a_ecef +=
            ecef_flux.dir * (a_coef * (ecef_flux.mag_sw + ecef_flux.mag_lw));
    }

    // Analytic RP could also calculate panel acceleration?
    // a_eci += compute_panel_accel(Flux::eci_fluxes, eci_attitude.panel);
    // a_ecef += compute_panel_accel(Flux::ecef_fluxes, ecef_attitude.panel);

    // a_eci and a_ecef above are the unscaled model. Publish them for the
    // sensitivity integration, then contribute the scaled acceleration.
    state->srp_unscaled_eci += a_eci;
    state->srp_unscaled_ecef += a_ecef;

    // A tumbling object presents a varying projected area. Modulating the
    // scale here lets a synthetic arc carry that variation, so a fit that
    // assumes a constant scale can be tested against one that does not hold.
    double applied = state->srp_scale;
    if (state->srp_scale_amp != 0.0 && state->srp_scale_period > 0.0) {
        const double dt =
            (state->eci.epoch.get_MJD_UTC() - state->srp_scale_t0) * 86400.0;
        applied *= 1.0 + state->srp_scale_amp *
                             std::sin(2.0 * M_PI * dt / state->srp_scale_period);
    }

    state->total_a_eci += applied * a_eci;
    state->total_a_ecef += applied * a_ecef;

    // --- empirical (ECOM) terms ---------------------------------------------
    // Classical ECOM resolves the residual radiation-pressure acceleration in
    // the sun-oriented DYB frame: D towards the Sun, Y along the solar panel
    // rotation axis, B completing the set, with a once-per-revolution term in
    // B. Column 0 of the sensitivity block is the scale on the a priori model
    // above, which plays the role of D0; these are the other four.
    //
    // The partials are just the unit directions, because the acceleration is
    // linear in each coefficient. All of them vanish in eclipse.
    const Cartesian e_D = state->eci_rso_sun_hat;
    const Cartesian r = Cartesian(state->eci.x, state->eci.y, state->eci.z);
    Cartesian e_Y = cross_product(e_D, r);

    if (e_Y.length() > 0.0 && state->eclipse_state > 0.0) {
        e_Y.normalise();
        const Cartesian e_B = cross_product(e_D, e_Y);

        // Argument of latitude, from the ascending node.
        const Cartesian h_hat = normalise(state->h);
        Cartesian node = cross_product(Cartesian(0.0, 0.0, 1.0), h_hat);
        double u = 0.0;
        if (node.length() > 0.0) {
            node.normalise();
            u = std::atan2(dot_product(r, cross_product(h_hat, node)),
                           dot_product(r, node));
        }

        const double sun = state->eclipse_state;
        state->emp_partial[1] = sun * e_Y;
        state->emp_partial[2] = sun * e_B;
        state->emp_partial[3] = (sun * std::cos(u)) * e_B;
        state->emp_partial[4] = (sun * std::sin(u)) * e_B;

        for (int q = 1; q < 5; ++q) {
            state->total_a_eci += state->emp_coeff[q] * state->emp_partial[q];
        }
    }
}
