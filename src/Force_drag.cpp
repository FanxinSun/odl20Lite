/*! @file Force_drag.cpp
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of atmospheric models and drag force.

    Atmospheric models should be spun off into their own class(es) in future.
 */

#include "../include/Force_drag.h"

void Force_drag::setup(const Resident_constants &rso_const,
                       std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    location = rso_const.location;
    GMTtime = rso_const.GMTtime;

    minus500C_dAm =
        -500.0 * rso_const.drag_coeff * rso_const.area / rso_const.mass;

    if (rso_const.drag_coeff <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_drag: spacecraft drag coefficient is 0 or negative, "
              << rso_const.drag_coeff;
        state->errors.push_back(error.str());
    }

    if (rso_const.area <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_drag: spacecraft area is 0 or negative, "
              << rso_const.area << " m^2.";
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_drag: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());
        minus500C_dAm = 0.0; // Prevent future NaN's
    }

    // state->atmos_density = 0;
}

void Force_drag::compute_acceleration()
{
    // Report error, this displays the issue to the user and halts simulation
    if (state->geodetic.alt < 100.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_drag: Altitude too low, " << state->geodetic.alt
              << " km.";
        state->errors.push_back(error.str());
    }

    double rho = state->atmos_density;

    // space-craft acceleration in km/s^2 in ECEF frame
    a_ecef = minus500C_dAm * rho * state->ecef_v * state->ecef_rso_vel;
    state->total_a_ecef += a_ecef;
}


// New code 30 Jan 2020
std::unique_ptr<Force> Force_drag::get_drag_model(int model_num) {
  std::unique_ptr<Force> drag_model;

  if (model_num == 3) {
    drag_model = std::make_unique<Force_drag_nrlmsise00>();
  } else if (model_num == 2) {
    drag_model = std::make_unique<Force_drag_tiegcm>();
  } else if (model_num == 1) {
    drag_model = std::make_unique<Force_drag_ussa76>();
  } else {
    drag_model = std::make_unique<Force_drag>();
  }
  return drag_model;
}
