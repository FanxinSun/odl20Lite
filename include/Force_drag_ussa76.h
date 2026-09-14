/*! @file Force_drag_ussa76.h
	@author Santosh Bhattarai
	@date 30 January 2020
	@brief UCL ODL implemenation of drag force model using USSA76
 */

#ifndef SGNL_FORCE_DRAG_USSA76_H
#define SGNL_FORCE_DRAG_USSA76_H

#include "Force_drag.h"

/*!
 * @class Force_drag_ussa76
 * @date 30 January 2020
 * @author Santosh Bhattarai
 * @brief Uses a spherically symmetric atmospheric density model that
 *		  exponentially decays with altitude
 */
class Force_drag_ussa76 : public Force_drag
{
public:
	// double minus500C_dAm = 0.0; // A bunch of constants rolled into one
    // int location = 0;           // Location to base atmospheric model
    // int GMTtime = 0;            // Time of day for atmospheric model
	static const std::vector<double> ussa1976_density_array;

	Force_drag_ussa76() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

    double get_density(double alt);

    /*!
     * @fn ussa1976_density(double alt)
     * @date 2 November 2015
     * @brief Returns density from 1976 USSA model at a given altitude.
     */
    double ussa1976_density(double alt);
};

#endif