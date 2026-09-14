/*! @file Force_lorentz.h
	@author David Harrison
	@date 15 November May 2016
	@brief SGNL OPS implementation of Earth gravity.
 */

#ifndef SGNL_FORCE_LORENTZ_H
#define SGNL_FORCE_LORENTZ_H

#include "Force.h"

/**
 * @class Force_lorentz
 * @date 15 November 2016
 * @author David Harrison
 * @brief Computes acceleration (in ECEF) due to Earth's magnetic field and
 *        spacecraft electrical charge using spherical harmonic based models.
 * <Montenbruck00> Satellite Orbits: Model, Methods and Applications,
 *		           Section 3.2 Geopotential.
 * <Thébault et al.> International Geomagnetic Reference Field: the 12th
 *                   generation
 *
 * This class uses a method originally based on Montenbruck but is much more
 * computationally efficient and not directly compatible with it.
 *
 * If we ever get documentation setup correctly, then I have a document about
 * this method that should be included with it. - David Harrison
 */
class Force_lorentz : public Force
{
  private:
    size_t degree = 0u; //!< Degree of magnetic field model
    size_t order = 0u;  //!< Order of magnetic field model
    std::vector<long int> MJDs = {};

    // For denormalised g' and h' coefficients (C and S from gravity notation)
    // coef[(year-1970)/5][n*(n+1)/2+m] and interpolate between first and second
    std::vector<std::vector<
        std::pair<Spherical_harmonic_coef, Spherical_harmonic_coef>>>
        coef = {};

    // For denormalised g' and h' coefficients (C and S from gravity notation)
    std::vector<Spherical_harmonic_coef> CS = {};

    Cartesian B;                  // Magnetic field
    double specific_charge = 0.0; // Measured in nano-Coulombs / kg

  public:
    Force_lorentz() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    // Uses V' and W', g' and h' coefs to compute accel partial derivatives
    void compute_partial_derivatives() const override;

    Cartesian get_Bnm(size_t n, size_t m) const;

    // Uses V' and W' evals and C' and S' coefs to compute acceleration
    void compute_acceleration() override;

  private:
    // Extract coefficients from file and pass them off to denorm_coef()
    void populate_magnetic_coefs(std::ifstream &infile);

    // Calculates g, used to denormalise coefficients read in from gravity model
    std::vector<std::vector<double>>
    generate_denorm_factors(size_t max_n, size_t max_m) const;

    // For testing purposes
    void verify_denorm_factors(size_t max_n) const;

    // Precomputate and populate C and S
    void
    denorm_coef(std::vector<std::vector<Spherical_harmonic_coef>> &temp_coef);
    void reformat_coef(
        const std::vector<std::vector<Spherical_harmonic_coef>> &temp_coef);

    void calculate_CS();
    void calculate_field();

    void output_field();
};

#endif
