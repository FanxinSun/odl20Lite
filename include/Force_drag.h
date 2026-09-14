/*! @file Force_drag.h
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of atmospheric models and drag force.

    Atmospheric models should be spun off into their own class(es) in future.
 */

#ifndef SGNL_FORCE_DRAG_H
#define SGNL_FORCE_DRAG_H

#include "Force.h"

/*!
 * @class Force_drag
 * @date 16 May 2016
 * @author David Harrison
 * @brief Computes acceleration (in ECEF) based on atmospheric drag
 *
 * Atmospheric models should be split off into their own classes, and drag model
 * should be updated to include drag from panels (angled to incoming flow).
 */
class Force_drag : public Force
{
  public:
    // Probably not going to need these. 
    double minus500C_dAm = 0.0; // A bunch of constants rolled into one
    int location = 0;           // Location to base atmospheric model
    int GMTtime = 0;            // Time of day for atmospheric model
   
    Force_drag() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

    // New code 30 Jan 2020
    static std::unique_ptr<Force> get_drag_model(int model_num);
};

#include "Force_drag_ussa76.h"
#include "Force_drag_tiegcm.h"
#include "Force_drag_nrlmsise00.h"

#endif
