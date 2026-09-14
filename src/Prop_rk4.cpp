/*! @file Prop_rk4.cpp
	@author David Harrison
	@date 19 January 2017
	@brief SGNL OPS class defining a Runge-Kutta 4 integrator using the 3/8 rule
 */

#include "../include/Prop_rk4.h"

void Prop_rk4::step()
{
    const State_vector Xo = rso.get_eci(); // Save initial state
    Matrix6x6 PhiMo = rso.phiM;            // Initial state transition matrix

    State_vector dXidt[4]; // Time derivatives of ith State Vector
    Matrix6x6 dPhidt[4];   // Time derivatives of ith Phi matrix.
    Matrix6x1 dSdt[4];     // Time derivatives of the SRP sensitivity column.

    const Matrix6x1 So = rso.srpS; // Initial SRP sensitivity, dy/dp

    Cartesian a = rso.get_acceleration();
    Matrix6x6 dfdy = rso.get_partial_derivatives();
    Cartesian dadp = rso.get_srp_partial();

    // d(S)/dt = dF/dy * S + [0; da/dp]: the sensitivity is driven by the
    // unscaled SRP acceleration entering through the velocity rows.
    Matrix6x1 forcing = Matrix6x1::Zero();

    // We set the epoch to Xo.epoch here so that all future Xi state vectors
    // will also be initialised to that value. But we don't bother setting the
    // epoch of the other dXidt State Vectors as we never need it.
    dXidt[0].set(Xo.u, Xo.v, Xo.w, a.x, a.y, a.z, Xo.epoch);
    dPhidt[0] = dfdy * PhiMo;
    forcing(3) = dadp.x; forcing(4) = dadp.y; forcing(5) = dadp.z;
    dSdt[0] = dfdy * So + forcing;

    // Arithmetic operations on State Vectors don't affect the epoch, therefore
    // Xf.epoch is also initialised to Xo.epoch here.
    State_vector Xf = h_b[0] * dXidt[0];
    Matrix6x6 PhiMf = static_cast<double>(h_b[0]) * dPhidt[0];
    Matrix6x1 Sf = static_cast<double>(h_b[0]) * dSdt[0];
    Xf.epoch.step(h_i, h_f); // Set epoch of the final state

    for (int i = 1; i < 4; ++i) {
        State_vector Xi = h_a[i][0] * dXidt[0]; // i-th state vector
        Matrix6x6 PhiMi = static_cast<double>(h_a[i][0]) * dPhidt[0];
        Matrix6x1 Si = static_cast<double>(h_a[i][0]) * dSdt[0];
        Xi.epoch.step(h_ci[i], h_cf[i]);

        for (int j = 1; j < i; ++j) {
            Xi += h_a[i][j] * dXidt[j];
            PhiMi += static_cast<double>(h_a[i][j]) * dPhidt[j];
            Si += static_cast<double>(h_a[i][j]) * dSdt[j];
        }

        Xi += Xo;
        PhiMi += PhiMo;
        Si += So;

        rso.update_with_acc_and_deriv(Xi);

        a = rso.get_acceleration();
        dfdy = rso.get_partial_derivatives();
        dadp = rso.get_srp_partial();

        dXidt[i].set(Xi.u, Xi.v, Xi.w, a.x, a.y, a.z);
        dPhidt[i] = dfdy * PhiMi;
        forcing(3) = dadp.x; forcing(4) = dadp.y; forcing(5) = dadp.z;
        dSdt[i] = dfdy * Si + forcing;

        Xf += h_b[i] * dXidt[i];
        PhiMf += static_cast<double>(h_b[i]) * dPhidt[i];
        Sf += static_cast<double>(h_b[i]) * dSdt[i];
    }

    // Add on initial state to achieve final state
    Xf += Xo;
    PhiMf += PhiMo;
    Sf += So;

    rso.phiM = PhiMf;
    rso.srpS = Sf;
    rso.update_with_acc_and_deriv(Xf);
}

// Populate rk4 coefficient arrays and combine with step size for efficiency
void Prop_rk4::set_step_size(long double step_size)
{
    Propagators::set_step_size(step_size);

    long double int_part_ld;
    long double h_c[4] = {};

    h_c[1] = h_LD / 3.0L;
    h_c[2] = h_LD / 1.5L;
    h_c[3] = h_LD;

    // Convert to integer and fractional parts to help maintain precision
    for (int i = 1; i < 4; ++i) {
        h_cf[i] = static_cast<double>(std::modf(h_c[i], &int_part_ld));
        h_ci[i] = static_cast<long int>(int_part_ld);
    }

    h_a[1][0] = h_LD / 3.0L;

    h_a[2][0] = -h_LD / 3.0L;
    h_a[2][1] = h_LD;

    h_a[3][0] = h_LD;
    h_a[3][1] = -h_LD;
    h_a[3][2] = h_LD;

    h_b[0] = h_LD * 0.125L;
    h_b[1] = h_LD * 0.375L;
    h_b[2] = h_LD * 0.375L;
    h_b[3] = h_LD * 0.125L;
}
