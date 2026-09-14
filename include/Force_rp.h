/*! @file Force_rp.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS header file defining the Force_rp base class.
 */

#ifndef SGNL_FORCE_RP_H
#define SGNL_FORCE_RP_H

#include "Force.h"

/*!
 * @class Force_rp
 * @date 20 May 2016
 * @author David Harrison
 * @brief Base class for all radiation pressure force models.
 *
 * Note that all radiation pressure classes that calculate acceleration due to
 * flux on the solar panels also update the solar_array.flux values used by the
 * Force_trr class.
 *
 * Acceleration calculations from a Force_trr object should only be run AFTER
 * that for Force_rp.
 */
class Force_rp : public Force
{
  protected:
    double panel_spec_f[2] = {};
    double panel_spec_n[2] = {};
    double panel_diff[2] = {};

    std::vector<std::unique_ptr<Flux>> flux_models;

    Cartesian compute_panel_accel(const std::vector<Fluxstruct> &fluxes,
                                  Cartesian panel_norm) const;

    void test_panel_accel(double lat_deg, double lon_deg) const;

    Force_rp() = default;

  private:
    // Rule of 5 since we need a virtual destructor
    Force_rp(const Force_rp &copy_from) = default;
    Force_rp &operator=(const Force_rp &copy_from) = default;
    Force_rp(Force_rp &&) = default;
    Force_rp &operator=(Force_rp &&) = default;

  public:
    virtual ~Force_rp() = default;

    virtual void setup(const Resident_constants &rso_const,
                       std::shared_ptr<Resident_variables> in_state) override;

    virtual void compute_acceleration() override;

    static std::unique_ptr<Force> get_rp_model(int model_num);
};

#include "Force_rp_analytic.h"
#include "Force_rp_box_wing.h"
#include "Force_rp_gridfile.h"

#endif
