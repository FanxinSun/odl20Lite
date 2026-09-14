/*! @file attitudeState.cpp
	@author Santosh Bhattarai
	@date 1 July 2015
	@brief SGNL OPS header file for defining the attitude state of a resident
		   space object.
 */

#include "../include/Attitude_state.h"

// Default constructor for Attitude_state
Attitude_state::Attitude_state()
{
    x_hat = Cartesian(1.0, 0.0, 0.0);
    y_hat = Cartesian(0.0, 1.0, 0.0);
    z_hat = Cartesian(0.0, 0.0, 1.0);
    panel = x_hat;
}

Attitude_state::Attitude_state(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                               Cartesian in_panel)
{
    set_norm(in_x, in_y, in_z, in_panel);
}

Attitude_state::Attitude_state(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                               Cartesian in_panel, Matrix3x3 in_dxdr,
                               Matrix3x3 in_dydr, Matrix3x3 in_dzdr,
                               Matrix3x3 in_dpaneldr)
{
    set(in_x, in_y, in_z, in_panel, in_dxdr, in_dydr, in_dzdr, in_dpaneldr);
}

void Attitude_state::set(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                         Cartesian in_panel)
{
    x_hat = in_x;
    y_hat = in_y;
    z_hat = in_z;
    panel = in_panel;

    dxdr << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
    dydr = dxdr;
    dzdr = dxdr;
    dpaneldr = dxdr;
}

// This method assumes that all the Cartesian vectors are normalised
void Attitude_state::set(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                         Cartesian in_panel, Matrix3x3 in_dxdr,
                         Matrix3x3 in_dydr, Matrix3x3 in_dzdr,
                         Matrix3x3 in_dpaneldr)
{
    x_hat = in_x;
    y_hat = in_y;
    z_hat = in_z;
    panel = in_panel;

    dxdr = in_dxdr;
    dydr = in_dydr;
    dzdr = in_dzdr;
    dpaneldr = in_dpaneldr;
}

// Set then normalise, allows setting the attitude state from non-unit vectors
void Attitude_state::set_norm(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                              Cartesian in_panel)
{
    set(in_x, in_y, in_z, in_panel);

    x_hat.normalise();
    y_hat.normalise();
    z_hat.normalise();
    panel.normalise();
}

// Print function for the Attitude_state elements for debugging
void Attitude_state::print()
{
    std::cout << "x_hat BFS:\n";
    x_hat.print();

    std::cout << "y_hat BFS:\n";
    y_hat.print();

    std::cout << "z_hat BFS:\n";
    z_hat.print();

    std::cout << "panel_hat BFS:\n";
    panel.print();
}

/**
 *  @cite: "GNSS satellite geometry and attitude models", O. Montenbruck, 2015
 *
 *  This function computes and returns the body-frame unit vectors as defined by
 *  the IGS Yaw Steering mode:
 *  (a) z-axis point down the boresight from the COM to the Earth's origin,
 *  (b) y-axis parallel to the solar panel yoke-arms,
 *  (c) the x-axis forms the right-handed coordinate basis.
 *
 *  IGS definition assumes that +x face is on sunlit side, but sometimes this is
 *  not the case. Therefore there is a flag to flip the x and y axes, such that
 *  the -x face is the sunlit side, as it is for GPS IIR and Galileo missions.
 */
void Attitude_state::set_yaw_steering(Cartesian rso, Cartesian sun,
                                      bool follow_IGS)
{
    // Define panel normal to point directly toward sun
    panel = sun - rso;

    // For the opposite definition to IGS where x and y axes are flipped, we can
    // achieve this by simply flipping the sun and leaving the code unchanged.
    if (!follow_IGS) {
        sun = -sun;
    }

    // Set body-frame z-direction from ECI state vector
    z_hat = -rso;

    // Define y perpendicular to z and sun vectors
    y_hat = cross_product(sun, rso);

    // Define x as perpendicular to y and z vectors
    x_hat = cross_product(y_hat, z_hat); // r cross (r cross s)

    Matrix3x3 rrT = col_times_row(rso, rso);
    Matrix3x3 rsT = col_times_row(rso, sun);

    double r_dot_s = rsT(0, 0) + rsT(1, 1) + rsT(2, 2);
    double s_mag2 = sun.length2();

    double x_len2 = x_hat.length2();
    double y_len2 = y_hat.length2();
    double z_len2 = rrT(0, 0) + rrT(1, 1) + rrT(2, 2);
    double panel_len2 = panel.length2();

    double x_len = std::sqrt(x_len2);
    double y_len = std::sqrt(y_len2);
    double z_len = std::sqrt(z_len2);
    double panel_len = std::sqrt(panel_len2);

    Cartesian partial_d =
        (r_dot_s * r_dot_s - z_len2 * s_mag2) * rso + (z_len2 * r_dot_s) * sun;

    // dxdr = d[(r cross (r cross s))/|r cross (r cross s)|]/dr
    dxdr = (rsT - 2.0 * rsT.transpose() + diagonal(r_dot_s) +
            col_times_row(x_hat, partial_d / x_len2)) /
           x_len;

    partial_d = cross_product(sun, y_hat) / (y_len2 * y_len);

    // dydr = d[(s cross r)/|s cross r|]/dr
    dydr = col_times_row(partial_d, y_hat);

    // dzdr = -d(r/|r|)/dr
    dzdr = (rrT - diagonal(z_len2)) / (z_len2 * z_len);

    // dpaneldr = d[(s - r)/|s - r|]/dr
    dpaneldr = (col_times_row(panel, panel) - diagonal(panel_len2)) /
               (panel_len2 * panel_len);

    // Normalise BFS vectors
    x_hat /= x_len;
    y_hat /= y_len;
    z_hat /= z_len;
    panel /= panel_len;
}
