/*! @file Force_trr.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of a solar panel Thermal Re-Radiation model.
 */

#include "../include/Force_trr.h"

void Force_trr::setup(const Resident_constants &rso_const,
                      std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    area = rso_const.solar_array.area;

    alpha[0] = rso_const.solar_array.alpha[0];
    alpha[1] = rso_const.solar_array.alpha[1];

    k[0] = rso_const.solar_array.k[0];
    k[1] = rso_const.solar_array.k[1];

    epsilon[0] = rso_const.solar_array.epsilon[0];
    epsilon[1] = rso_const.solar_array.epsilon[1];

    delta[0] = rso_const.solar_array.thickness[0];
    delta[1] = rso_const.solar_array.thickness[1];

    commanded_power = rso_const.solar_array.power;

    a_coef = area * sgnlOPS::sigmaSB / (1500.0 * sgnlOPS::c * rso_const.mass);

    if (area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: spacecraft solar panel area is 0 or negative, "
              << area << " m^2.";
        state->errors.push_back(error.str());
        area = 1.0; // Prevent future NaN's
    }

    if (alpha[0] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr: first solar panel surface absorbtivity is negative, "
            << alpha[0];
        state->errors.push_back(error.str());
    }

    if (alpha[1] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: second solar panel surface absorbtivity is "
                 "negative, "
              << alpha[1];
        state->errors.push_back(error.str());
    }

    if (k[0] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: first solar panel layer conductivity is 0 or "
                 "negative, "
              << k[0] << " W/m/K.";
        state->errors.push_back(error.str());
    }

    if (k[1] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: second solar panel layer conductivity is 0 or "
                 "negative, "
              << k[1] << " W/m/K.";
        state->errors.push_back(error.str());
    }

    if (epsilon[0] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: first solar panel surface emissivity is negative, "
              << epsilon[0];
        state->errors.push_back(error.str());
    }

    if (epsilon[1] < 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr: second solar panel surface emissivity is negative, "
            << epsilon[1];
        state->errors.push_back(error.str());
    }

    if (delta[0] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error
            << "Force_trr: first solar panel layer thickness is 0 or negative, "
            << delta[0] << " m.";
        state->errors.push_back(error.str());
        delta[0] = 1.0; // Prevent future NaN's
    }

    if (delta[1] <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: second solar panel layer thickness is 0 or "
                 "negative, "
              << delta[1] << " m.";
        state->errors.push_back(error.str());
        delta[1] = 1.0; // Prevent future NaN's
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_trr: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());
        a_coef = 0.0; // Prevent future NaN's
    }
}

// Convention: let the 0 element of arrays be the sunlit side of the panel.
void Force_trr::panel_surface_temps()
{
    // Stu's algorithm for TRR temperature calculation.

    //  A Matrix = sensitivity of residuals wrt temperatures.
    double a11, a12, a21, a22, a23, a32, a33;

    // B Vector of residuals (Watts/m^2).
    double b1, b2, b3;

    // Cofactor matrix elements.
    double c11, c12, c13, c21, c22, c23, c31, c32, c33;

    double determinant_A;

    double X[3]; // temperature corrections (Delta B = -B).

    double E[2];
    E[0] = state->panel_flux_sw[0] + state->panel_flux_lw[0];
    E[1] = state->panel_flux_sw[1] + state->panel_flux_lw[1];

    //std::cout << "Outside- E[0] " << E[0] << std::endl;

    // Assume 15% efficient panel, collecting SW radiation
    double max_elec_power = 0.15 * alpha[0] * state->panel_flux_sw[0] * area;

    double power = std::min(max_elec_power, commanded_power);

    // Initialize temperature estimates:
    temp[0] = 315;
    temp[1] = 315;
    temp[2] = 308;
    // Calculate initial residuals:
    b1 = -(alpha[0] * E[0]) +
         sgnlOPS::sigmaSB * epsilon[0] * pow(temp[0], 4.0) +
         (k[0] * (temp[0] - temp[1]) / delta[0]);
    b2 = power / area + k[0] * (temp[1] - temp[0]) / delta[0] +
         k[1] * (temp[1] - temp[2]) / delta[1];
    b3 = -(alpha[1] * E[1]) +
         sgnlOPS::sigmaSB * epsilon[1] * pow(temp[2], 4.0) +
         (k[1] * (temp[2] - temp[1]) / delta[1]);
    double tol = 1e-10; // tolerance on residuals for completing iterations.
    // Iterate to converge on steady-state temperatures:
    // Prevent infinite loop if tolerance is set too tightly for convergence.
    int breakoutcounter = 0;
    while ((fabs(b1) > tol || fabs(b2) > tol || fabs(b3) > tol) &&
           breakoutcounter < 10) {
        // Elements of A matrix: (A*T=b)
        a11 = (4.0 * sgnlOPS::sigmaSB * epsilon[0] * pow(temp[0], 3.0)) +
              (k[0] / delta[0]);
        a12 = -k[0] / delta[0];
        a21 = -k[0] / delta[0];
        a22 = k[0] / delta[0] + k[1] / delta[1];
        a23 = -k[1] / delta[1];
        a32 = -k[1] / delta[1];
        a33 = (4.0 * sgnlOPS::sigmaSB * epsilon[1] * pow(temp[2], 3.0)) +
              (k[1] / delta[1]);
        // Cofactor matrix of A:
        c11 = a22 * a33 - a32 * a23;
        c12 = -a21 * a33;
        c13 = a21 * a32;
        c21 = -a12 * a33;
        c22 = a11 * a33;
        c23 = -a11 * a32;
        c31 = a12 * a23;
        c32 = -a11 * a23;
        c33 = a11 * a22 - a21 * a12;
        determinant_A = a11 * c11 + a12 * c12;
        // Temperature corrections are a linear combination of residuals (Delta B=C^T*A/(det A)):
        X[0] = (c11 * b1 + c21 * b2 + c31 * b3) / determinant_A;
        X[1] = (c12 * b1 + c22 * b2 + c32 * b3) / determinant_A;
        X[2] = (c13 * b1 + c23 * b2 + c33 * b3) / determinant_A;
        // New temperature estimates:
        temp[0] = temp[0] - X[0];
        temp[1] = temp[1] - X[1];
        temp[2] = temp[2] - X[2];

        // Calculate residuals for determining when convergence has occurred.
        b1 = -(alpha[0] * E[0]) +
             sgnlOPS::sigmaSB * epsilon[0] * pow(temp[0], 4.0) +
             (k[0] * (temp[0] - temp[1]) / delta[0]);
        b2 = power / area + k[0] * (temp[1] - temp[0]) / delta[0] +
             k[1] * (temp[1] - temp[2]) / delta[1];
        b3 = -(alpha[1] * E[1]) +
             sgnlOPS::sigmaSB * epsilon[1] * pow(temp[2], 4.0) +
             (k[1] * (temp[2] - temp[1]) / delta[1]);
        breakoutcounter++;
        //printf("Residuals:\t%12e\t%12e\t%12e\n", b1,b2,b3);
    } // end of iteration loop in TRR temperature algorithm.

    // std::cout << temp[0] << ", " << temp[1] << ", " << temp[2] << std::endl;

    //std::cout << "computePanelTemps " <<  temp[0] << " " << temp[0]-temp[2] << std::endl;
}

void Force_trr::compute_acceleration()
{
    double a_mag = 0.0;

    if (state->panel_flux_sw[0] > 0 || state->panel_flux_sw[1] > 0 ||
        state->panel_flux_lw[0] > 0 || state->panel_flux_lw[1] > 0) {
        panel_surface_temps();

        double eT4_0, eT4_1;

        eT4_0 = temp[0] * temp[0];
        eT4_0 *= eT4_0 * epsilon[0];

        eT4_1 = temp[2] * temp[2];
        eT4_1 *= eT4_1 * epsilon[1];

        // Magnitude of acceleration contribution due to diffuse TRR.
        a_mag = a_coef * (eT4_1 - eT4_0);

        // Resultant acceleration.
        // TRR acceleration is in opposite direction of panel sunlit normal.
        a_ecef = state->ecef_attitude.panel * a_mag;
        state->total_a_ecef += a_ecef;
    }

    /*
	 For study on the thermal force, need as output:
	 front_temp, back_temp, emitted_radiation, thermal_force, thermal_acc
	 */

    // std::cout << temp[0] << ", "
    // 		   << temp[1] << ", "
    // 		   << temp[2] << ", "
    // 		   << temp[0]-temp[2] << ", "
    // 		   << area << ", "
    // 		   << a.length() << ", "
    // 		   << a.x << ", "
    // 		   << a.y << ", "
    // 		   << a.z << "\n";
}
