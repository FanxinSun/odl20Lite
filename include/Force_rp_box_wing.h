/*! @file Force_rp_box_wing.h
	@author David Harrison
	@date 21 May 2016
	@brief SGNL OPS implementation of a box and wing radiation pressure model.
 */

#ifndef SGNL_FORCE_RP_BOX_WING_H
#define SGNL_FORCE_RP_BOX_WING_H

#include "Force_rp.h"

/*!
 * @class Force_rp_box_wing
 * @date 21 May 2016
 * @author David Harrison
 * @brief Uses a simple analytic model to calculate acceleration due to incoming
 *        flux (in W/m^2).
 */
class Force_rp_box_wing : public Force_rp
{
  private:
    Cartesian spec_f_pos;
    Cartesian spec_n_pos;
    Cartesian diff_pos;
    Cartesian spec_f_neg;
    Cartesian spec_n_neg;
    Cartesian diff_neg;

  public:
    Force_rp_box_wing() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

    Cartesian compute_box_accel(const std::vector<Fluxstruct> &fluxes,
                                const Attitude_state &attitude) const;

    void test_box_accel(double lat_deg, double lon_deg) const;
};

#endif
