/*! @file Force_earth_gravity.h
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of Earth gravity.
 */

#ifndef SGNL_FORCE_EARTH_GRAVITY_H
#define SGNL_FORCE_EARTH_GRAVITY_H

#include "Force.h"

/**
 * @class Force_earth_gravity
 * @date 16 May 2016
 * @author David Harrison
 * @brief Computes acceleration (in ECEF) due to Earth gravity using spherical
 *        harmonic based models.
 * <Montenbruck00> Satellite Orbits: Model, Methods and Applications,
 *		           Section 3.2 Geopotential.
 *
 * This class uses a method originally based on Montenbruck but is much more
 * computationally efficient and not directly compatible with it.
 *
 * If we ever get documentation setup correctly, then I have a document about
 * this method that should be included with it. - David Harrison
 *
 * As this model populates a value for GM, which is used by other forces, it
 * should always be the first force to be instantiated.
 */
class Force_earth_gravity : public Force
{
  private:
    double GM = sgnlOPS::GM;
    size_t degree = 0u; //!< Degree of gravity model
    size_t order = 0u;  //!< Order of gravity model

    Timetag epoch_timevar;
    size_t degree_timevar = 0u; //!< Degree of time variable gravity model
    size_t order_timevar = 0u;  //!< Order of time variable gravity model

    size_t degree_tide = 0u; //!< Degree of earth tide gravity model
    size_t order_tide = 0u;  //!< Order of earth tide gravity model

    // For denormalised C' and S' coefficients
    std::vector<Spherical_harmonic_coef> CS;
    std::vector<Spherical_harmonic_coef> CS_orig;
    std::vector<Spherical_harmonic_coef> CS_timevar;

  public:
    Force_earth_gravity() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    double compute_potential();
    double compute_potential(size_t max_n);
    double compute_potential_degree_only(size_t max_n);

    // Uses V' and W', C' and S' coefs to compute accel partial derivatives
    void compute_partial_derivatives() const override;

    Cartesian get_a(size_t n, size_t m) const;

    // Uses V' and W' evals and C' and S' coefs to compute acceleration
    void compute_acceleration() override;

    Cartesian compute_fast_acceleration(State_vector eci) const override;

    // Returns value for GM for use by other force models
    double get_GM();

  private:
    // Extract coefficients from file and pass them off to denorm_coef()
    // Also sets value for GM and Re on a model specific bases
    void populate_gravity_coefs(int grav_num, std::ifstream &infile);

    // Calculates g, used to denormalise coefficients read in from gravity model
    std::vector<std::vector<double>>
    generate_denorm_factors(size_t max_n, size_t max_m) const;

    // For testing purposes
    void verify_denorm_factors(size_t in_n) const;

    // Precomputate and populate C and S
    std::vector<Spherical_harmonic_coef>
    denorm_coef(const std::vector<Spherical_harmonic_coef> &norm);

    void update_gravity_coefficients();

    Spherical_harmonic_coef solid_earth_pole_tide_1996();
    Spherical_harmonic_coef solid_earth_pole_tide_2010();
};

#endif
