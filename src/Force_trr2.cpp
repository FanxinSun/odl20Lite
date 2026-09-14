/*! @file Force_trr2.cpp
	@author David Harrison
	@date 21 May 2016
	@brief SGNL OPS implementation of a solar panel Thermal Re-Radiation model.
 */

#include "../include/Force_trr2.h"

void Force_trr2::setup(const Resident_constants &rso_const,
                       std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    a_coef =
        rso_const.solar_array.area / (1500.0 * sgnlOPS::c * rso_const.mass);

    sigep[0] = sgnlOPS::sigmaSB * rso_const.solar_array.epsilon[0];
    sigep[1] = sgnlOPS::sigmaSB * rso_const.solar_array.epsilon[1];
    k_over_x =
        1.0 / (rso_const.solar_array.thickness[0] / rso_const.solar_array.k[0] +
               rso_const.solar_array.thickness[1] / rso_const.solar_array.k[1]);

    powered_area = rso_const.solar_array.power_frac;

    commanded_power_den = rso_const.solar_array.power /
                          (powered_area * rso_const.solar_array.area);

    if (powered_area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: spacecraft solar array powered area fraction is "
                 "0 or negative, "
              << powered_area;
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: spacecraft solar array area is 0 or negative, "
              << rso_const.solar_array.area << " m^2.";
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.power <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: spacecraft solar array power draw is 0 or "
                 "negative, "
              << rso_const.solar_array.power << " W.";
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.thickness[0] +
            rso_const.solar_array.thickness[1] <=
        0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: total solar panel thickness is 0 or negative, "
              << rso_const.solar_array.thickness[0] +
                     rso_const.solar_array.thickness[1]
              << " m.";
        state->errors.push_back(error.str());
        k_over_x = 1.0E10;
    }

    if (rso_const.solar_array.k[0] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: first solar panel layer conductivity is 0 or "
                 "negative, "
              << rso_const.solar_array.k[0] << " W/m/K.";
        state->errors.push_back(error.str());
        k_over_x = 0.0; // Prevent future NaN's
    }

    if (rso_const.solar_array.k[1] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: second solar panel layer conductivity is 0 or "
                 "negative, "
              << rso_const.solar_array.k[1] << " W/m/K.";
        state->errors.push_back(error.str());
        k_over_x = 0.0; // Prevent future NaN's
    }

    if (rso_const.solar_array.epsilon[0] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr2: first solar panel surface emissivity is negative, "
            << rso_const.solar_array.epsilon[0];
        state->errors.push_back(error.str());
    }

    if (rso_const.solar_array.epsilon[1] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr2: second solar panel surface emissivity is negative, "
            << rso_const.solar_array.epsilon[1];
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr2: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());
        a_coef = 0.0; // Prevent future NaN's
    }

    k_over_x2 = k_over_x * k_over_x;

    double refl = 0.0;
    double area = 0.0;

    for (const auto &material : rso_const.solar_array.front) {
        refl += material.area * material.refl;
        area += material.area;
    }

    if (area > 0.0) {
        alpha_sw[0] = 1.0 - refl / area;
    } else {
        alpha_sw[0] = rso_const.solar_array.alpha[0];
    }

    refl = 0.0;
    area = 0.0;

    for (const auto &material : rso_const.solar_array.rear) {
        refl += material.area * material.refl;
        area += material.area;
    }

    if (area > 0.0) {
        alpha_sw[1] = 1.0 - refl / area;
    } else {
        alpha_sw[1] = rso_const.solar_array.alpha[1];
    }

    alpha_lw[0] = rso_const.solar_array.epsilon[0];
    alpha_lw[1] = rso_const.solar_array.epsilon[1];

    // These two lines force the short wave absorptivity to be that set in
    // solar_array, regardless of other material properties. This is what TRR
    // model 1 assumes, so uncommenting these lines can give a more direct
    // comparison between the two.
    // alpha_sw[0] = rso_const.solar_array.alpha[0];
    // alpha_sw[1] = rso_const.solar_array.alpha[1];
}

// Convention: let the 0 element of arrays be front of the panel.
// This function returns sigma*epsilon1*T1^4 - sigma*epsilon0*T0^4
// That is, the difference in emitted power density between the front and back.
double Force_trr2::emitted_power_density(double E0, double E1)
{
    double k_over_x_dt;

    // Jacobian matrix, sensitivity of residuals wrt temperatures.
    double a[2], inv_det_a;

    // We don't need to store a[0][1] or a[1][0], but it should look like this:
    // a[0][0] = 4.0 * sigep_T2[0] * T[0] + k_over_x;
    // a[0][1] = -k_over_x;
    // a[1][0] = -k_over_x;
    // a[1][1] = 4.0 * sigep_T2[1] * T[1] + k_over_x;

    // B Vector of residuals (Watts/m^2).
    double b[2];

    // Tolerance on residuals for completing iterations.
    double tol = 1e-10;

    // Initial guess for panel surface temperatures
    double T[2];
    T[0] = 315.0;
    T[1] = 308.0;

    double T2[2], sigep_T2[2], sigep_T4[2], dT[2];

    T2[0] = T[0] * T[0];
    T2[1] = T[1] * T[1];

    sigep_T2[0] = sigep[0] * T2[0];
    sigep_T2[1] = sigep[1] * T2[0];

    sigep_T4[0] = sigep_T2[0] * T2[0];
    sigep_T4[1] = sigep_T2[1] * T2[1];

    // Prevent infinite loop if tolerance is set too tightly for convergence.
    int iter = 0;

    // Iterate to converge on steady-state temperatures using Newton's method
    // and Jacobian matrix of partials.
    do {
        a[0] = 4.0 * sigep_T2[0] * T[0] + k_over_x;
        a[1] = 4.0 * sigep_T2[1] * T[1] + k_over_x;

        inv_det_a = 1.0 / (a[0] * a[1] - k_over_x2);

        k_over_x_dt = k_over_x * (T[0] - T[1]);

        b[0] = sigep_T4[0] + k_over_x_dt - E0;
        b[1] = sigep_T4[1] - k_over_x_dt - E1;

        dT[0] = (a[1] * b[0] + k_over_x * b[1]) * inv_det_a;
        dT[1] = (a[0] * b[1] + k_over_x * b[0]) * inv_det_a;

        // New temperature estimates:
        T[0] -= dT[0];
        T[1] -= dT[1];

        T2[0] = T[0] * T[0];
        T2[1] = T[1] * T[1];

        sigep_T2[0] = sigep[0] * T2[0];
        sigep_T2[1] = sigep[1] * T2[1];

        sigep_T4[0] = sigep_T2[0] * T2[0];
        sigep_T4[1] = sigep_T2[1] * T2[1];
    } while ((std::abs(dT[0]) > tol || std::abs(dT[1]) > tol) && ++iter < 10);

    // std::cout << T[0] << ", " << T[1] << std::endl;

    return (sigep_T4[1] - sigep_T4[0]);
}

// Modified algorithm for TRR temperature calculation.
void Force_trr2::compute_acceleration()
{
    if (state->panel_flux_sw[0] > 0 || state->panel_flux_sw[1] > 0 ||
        state->panel_flux_lw[0] > 0 || state->panel_flux_lw[1] > 0) {

        double max_power_den = alpha_sw[0] * state->panel_flux_sw[0];

        double E0, E1;
        E0 = max_power_den + alpha_lw[0] * state->panel_flux_lw[0];
        E1 = alpha_sw[1] * state->panel_flux_sw[1] +
             alpha_lw[1] * state->panel_flux_lw[1];

        // Assume 15% efficient panel, collecting SW radiation
        max_power_den *= 0.15;

        double a_mag = 0.0;

        // Magnitude of acceleration contribution due to diffuse TRR.
        // First calculate for part of panel with no power drawn
        if (powered_area < 1.0) {
            a_mag += emitted_power_density(E0, E1) * (1.0 - powered_area);
        }

        // Update effective flux
        E0 -= std::min(max_power_den, commanded_power_den);

        // Now calculate for part of panel with power drawn
        if (powered_area > 0.0) {
            a_mag += emitted_power_density(E0, E1) * powered_area;
        }

        a_mag *= a_coef;

        // Calculate acceleration assuming diffuse emission along panel normal
        a_ecef = state->ecef_attitude.panel * a_mag;
        state->total_a_ecef += a_ecef;
    }
}
