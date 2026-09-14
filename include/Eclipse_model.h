/*! @file Eclipse_model.h
	@author David Harrison
	@date 18 January 2017
	@brief SGNL OPS header file defining the Eclipse_model class.
 */

#ifndef SGNL_ECLIPSE_MODEL_H
#define SGNL_ECLIPSE_MODEL_H

#include "Cartesian.h"
#include "constants.h"

/*!
 * @class Eclipse_model
 * @date 18 January 2017
 * @author David Harrison
 * @brief Abstract base class for an eclipse model, may evolve into a base class
          in future as more eclipse models are added.
 */
class Eclipse_model
{
  public:
    static double eclipse(double rso_sun_distance, double eci_sun_distance,
                          Cartesian rso, Cartesian sun);

  private:
    static double sun_edge_earth_intersection(Cartesian rso, Cartesian edge);
    static Cartesian get_earth_edge(Cartesian rso, Cartesian sun);
    static double penumbra_flux_scale(Cartesian rso, Cartesian earth_edge,
                                      Cartesian sun, Cartesian sun_edge);
    static double penumbra_flux_scale(Cartesian blocked_edge,
                                      Cartesian earth_edge,
                                      Cartesian visible_edge);
};

#endif
