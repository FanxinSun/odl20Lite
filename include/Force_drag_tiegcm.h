/*! @file Force_drag_tiegcm.h
	@author Alex Forsyth & S. Bhattarai
	@date 30 January 2020
	@brief UCL ODL implemenation of drag force model using TIE-GCM
 */

#ifndef SGNL_FORCE_DRAG_TIEGCM_H
#define SGNL_FORCE_DRAG_TIEGCM_H

#include "Force_drag.h"

/*!
 * @class Force_drag_tiegcm
 * @date 30 January 2020
 * @author Alex Forsyth & S. Bhattarai
 * @brief Uses the Thermosphere-Ionoshpere-Electrodynamics General Circulation
 * Model (TIE-GCM) for the atmospheric density
 */
class Force_drag_tiegcm : public Force_drag
{
public:
    // Member variable
    std::vector<std::vector<double> > heightfile;
    std::vector<std::vector<double> > densityfile; 

    //! Set once the first out-of-range lookup has been reported, so a long
    //! simulation outside the grid does not flood the console.
    bool reported_out_of_range = false;
	
    // Constructor
    Force_drag_tiegcm() = default;

    // Member Functions
    void loadfiles();

    double interpolate(double latitude, double longitude, double time, std::vector<double> v);
    double get_density(double time, double altitude, double latitude, double longitude);

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;    
    void compute_acceleration() override;
};

#endif