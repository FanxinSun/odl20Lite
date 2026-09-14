/*! @file Flux.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS header file defining the Flux class.
 */

#ifndef SGNL_FLUX_H
#define SGNL_FLUX_H

#include <memory>

#include "Resident_variables.h"

/*!
 * @class Flux
 * @date 20 May 2016
 * @author David Harrison
 * @brief Abstract base class for all flux models.
 */
class Flux
{
  protected:
    Flux() = default;

    std::shared_ptr<Resident_variables> state; // Variables for force models

  private:
    // Rule of 5 since we need a virtual destructor
    Flux(const Flux &copy_from) = default;
    Flux &operator=(const Flux &copy_from) = default;
    Flux(Flux &&) = default;
    Flux &operator=(Flux &&) = default;

  public:
    virtual ~Flux() = default;

    virtual void add_flux_num(size_t &eci_flux_num,
                              size_t &ecef_flux_num) = 0;

    static std::vector<std::unique_ptr<Flux>>
    get_flux_models(int solar_flux_model, int earth_flux_model);

    virtual void compute_flux() = 0;

    virtual void setup(const std::shared_ptr<Resident_variables> &state);
};

// These includes are at the end because the classes in them extend this one.
// The get_flux_models method in this class refers to them.
#include "Flux_earth_ceres.h"
#include "Flux_earth_ecef.h"
#include "Flux_earth_eci.h"
#include "Flux_solar_ecef.h"
#include "Flux_solar_eci.h"

#endif
