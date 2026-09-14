/*! @file Force_trr3.cpp
	@author David Harrison
	@date 21 May 2016
	@brief SGNL OPS implementation of a solar panel Thermal Re-Radiation model.
 */

#include "../include/Force_trr3.h"

void Force_trr3::setup(const Resident_constants &rso_const,
                       std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    a_coef =
        rso_const.solar_array.area / (1500.0 * sgnlOPS::c * rso_const.mass);

    sigep[0] = sgnlOPS::sigmaSB * rso_const.solar_array.epsilon[0];
    sigep[1] = sgnlOPS::sigmaSB * rso_const.solar_array.epsilon[1];

    k_x[0] = rso_const.solar_array.k[0] / rso_const.solar_array.thickness[0];
    k_x[1] = rso_const.solar_array.k[1] / rso_const.solar_array.thickness[1];

    powered_area = rso_const.solar_array.power_frac;

    commanded_power_den = rso_const.solar_array.power /
                          (powered_area * rso_const.solar_array.area);

    if (powered_area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: spacecraft solar array powered area fraction is "
                 "0 or negative, "
              << powered_area;
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: spacecraft solar array area is 0 or negative, "
              << rso_const.solar_array.area << " m^2.";
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.power <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: spacecraft solar array power draw is 0 or "
                 "negative, "
              << rso_const.solar_array.power << " W.";
        state->errors.push_back(error.str());
        commanded_power_den = 0.0;
    }

    if (rso_const.solar_array.k[0] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: first solar panel layer conductivity is 0 or "
                 "negative, "
              << rso_const.solar_array.k[0] << " W/m/K.";
        state->errors.push_back(error.str());
    }

    if (rso_const.solar_array.k[1] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: second solar panel layer conductivity is 0 or "
                 "negative, "
              << rso_const.solar_array.k[1] << " W/m/K.";
        state->errors.push_back(error.str());
    }

    if (rso_const.solar_array.thickness[0] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: first solar panel layer thickness is 0 or "
                 "negative, "
              << rso_const.solar_array.thickness[0] << " m.";
        state->errors.push_back(error.str());
        k_x[0] = 1.0E10; // Prevent future NaN's
    }

    if (rso_const.solar_array.thickness[1] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: second solar panel layer thickness is 0 or "
                 "negative, "
              << rso_const.solar_array.thickness[1] << " m.";
        state->errors.push_back(error.str());
        k_x[1] = 1.0E10; // Prevent future NaN's
    }

    if (rso_const.solar_array.epsilon[0] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr3: first solar panel surface emissivity is negative, "
            << rso_const.solar_array.epsilon[0];
        state->errors.push_back(error.str());
    }

    if (rso_const.solar_array.epsilon[1] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr3: second solar panel surface emissivity is negative, "
            << rso_const.solar_array.epsilon[1];
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr3: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());
        a_coef = 0.0; // Prevent future NaN's
    }

    k_x2[0] = k_x[0] * k_x[0];
    k_x2[1] = k_x[1] * k_x[1];

    double refl = 0.0;
    double area = 0.0;

    for (const auto &material : rso_const.solar_array.front) {
        refl += material.area * material.refl;
        area += material.area;
    }

    if (area > 0.0) {
        alpha[0] = 1.0 - refl / area;
    } else {
        alpha[0] = rso_const.solar_array.alpha[0];
    }

    refl = 0.0;
    area = 0.0;

    for (const auto &material : rso_const.solar_array.rear) {
        refl += material.area * material.refl;
        area += material.area;
    }

    if (area > 0.0) {
        alpha[1] = 1.0 - refl / area;
    } else {
        alpha[1] = rso_const.solar_array.alpha[1];
    }

    epsilon[0] = rso_const.solar_array.epsilon[0];
    epsilon[1] = rso_const.solar_array.epsilon[1];

    // These two lines force the short wave absorptivity to be that set in
    // solar_array, regardless of other material properties. This is what TRR
    // model 1 assumes, so uncommenting these lines can give a more direct
    // comparison between the two.
    // alpha[0] = rso_const.solar_array.alpha[0];
    // alpha[1] = rso_const.solar_array.alpha[1];
}

// Convention: let the 0 element of arrays be front of the panel.
// This function returns sigma*epsilon1*T2^4 - sigma*epsilon0*T0^4
// That is, the difference in emitted power density between the front and back.
double Force_trr3::emitted_power_density(double E0, double E1, double E2)
{
    // Jacobian matrix, sensitivity of residuals wrt temperatures.
    double a[2], inv_det_a;

    // We don't need to store all of the A matrix, but it should look like this:
    // a[0][0] = 4.0 * sigep_T2[0] * T[0] + k_x[0];
    // a[0][1] = -k_x[0];
    // a[0][2] = 0.0;

    // a[1][0] = a[0][1];
    // a[1][1] = k_x[0] + k_x[1];
    // a[1][2] = -k_x[1];

    // a[2][0] = 0.0;
    // a[2][1] = a[1][2];
    // a[2][2] = 4.0 * sigep_T2[2] * T[2] + k_x[1];

    // B Vector of residuals (Watts/m^2).
    double b[3];

    double c[3][3] = {};

    // Tolerance on residuals for completing iterations.
    double tol = 1e-10;

    // Initial guess for panel surface temperatures
    double T[3];
    T[0] = 315.0;
    T[1] = 315.0;
    T[2] = 308.0;

    double T2[3], sigep_T2[3], sigep_T4[3], dT[3];

    T2[0] = T[0] * T[0];
    T2[1] = 0.0;
    T2[2] = T[2] * T[2];

    sigep_T2[0] = sigep[0] * T2[0];
    sigep_T2[1] = 0.0;
    sigep_T2[2] = sigep[1] * T2[2];

    sigep_T4[0] = sigep_T2[0] * T2[0];
    sigep_T4[1] = 0.0;
    sigep_T4[2] = sigep_T2[2] * T2[2];

    double k_x_sum = k_x[0] + k_x[1];

    // The rest of the C matrix is computed in the loop.
    c[0][2] = k_x[0] * k_x[1];
    // c[2][0] = c[0][2];

    // Prevent infinite loop if tolerance is set too tightly for convergence.
    int iter = 0;

    // Iterate to converge on steady-state temperatures using Newton's method
    // and Jacobian matrix of partials.
    do {
        a[0] = 4.0 * sigep_T2[0] * T[0] + k_x[0];
        a[1] = 4.0 * sigep_T2[2] * T[2] + k_x[1];

        b[0] = k_x[0] * (T[0] - T[1]);
        b[2] = k_x[1] * (T[2] - T[1]);

        b[1] = -(b[0] + b[2] + E1);

        b[0] += sigep_T4[0] - E0;
        b[2] += sigep_T4[2] - E2;

        c[0][0] = a[1] * k_x_sum - k_x2[1];
        c[0][1] = k_x[0] * a[1];

        // c[1][0] = c[0][1];
        c[1][1] = a[0] * a[1];
        c[1][2] = a[0] * k_x[1];

        // c[2][1] = c[1][2];
        c[2][2] = a[0] * k_x_sum - k_x2[0];

        inv_det_a = 1.0 / (a[0] * c[0][0] - k_x[0] * c[0][1]);

        dT[0] = (c[0][0] * b[0] + c[0][1] * b[1] + c[0][2] * b[2]) * inv_det_a;
        dT[1] = (c[0][1] * b[0] + c[1][1] * b[1] + c[1][2] * b[2]) * inv_det_a;
        dT[2] = (c[0][2] * b[0] + c[1][2] * b[1] + c[2][2] * b[2]) * inv_det_a;

        // New temperature estimates:
        T[0] -= dT[0];
        T[1] -= dT[1];
        T[2] -= dT[2];

        T2[0] = T[0] * T[0];
        T2[2] = T[2] * T[2];

        sigep_T2[0] = sigep[0] * T2[0];
        sigep_T2[2] = sigep[1] * T2[2];

        sigep_T4[0] = sigep_T2[0] * T2[0];
        sigep_T4[2] = sigep_T2[2] * T2[2];
    } while ((std::abs(dT[0]) > tol || std::abs(dT[1]) > tol ||
              std::abs(dT[2]) > tol) &&
             ++iter < 10);

    // std::cout << E0 << ", " << E1 << ", " << E2 << ", " << static_cast<int>(iter) << std::endl;
    // std::cout << T[0] << ", " << T[1] << ", " << T[2] << std::endl;
    // std::cout << "---------------------------" << std::endl;

    return (sigep_T4[2] - sigep_T4[0]);
}

// Modified algorithm for TRR temperature calculation.
void Force_trr3::compute_acceleration()
{
    if (state->panel_flux_sw[0] > 0 || state->panel_flux_sw[1] > 0 ||
        state->panel_flux_lw[0] > 0 || state->panel_flux_lw[1] > 0) {

        double E0, E1, E2;
        E0 = epsilon[0] * state->panel_flux_lw[0];
        E1 = alpha[0] * state->panel_flux_sw[0];
        E2 = epsilon[1] * state->panel_flux_lw[1] +
             alpha[1] * state->panel_flux_sw[1];

        double a_mag = 0.0;

        // Magnitude of acceleration contribution due to diffuse TRR.
        // First calculate for part of panel with no power drawn
        if (powered_area < 1.0) {
            a_mag += emitted_power_density(E0, E1, E2) * (1.0 - powered_area);
        }

        // Now calculate for part of panel with power drawn
        if (powered_area > 0.0) {
            // Assume 15% efficient panel, collecting SW radiation
            double max_power_den = E1 * 0.15;

            // Update effective flux
            E1 -= std::min(max_power_den, commanded_power_den);

            a_mag += emitted_power_density(E0, E1, E2) * powered_area;
        }

        a_mag *= a_coef;

        // Calculate acceleration assuming diffuse emission along panel normal
        a_ecef = state->ecef_attitude.panel * a_mag;
        state->total_a_ecef += a_ecef;
    }
}
