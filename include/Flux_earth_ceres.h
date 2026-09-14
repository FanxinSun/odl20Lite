/*! @file Flux_earth_ceres.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS header file defining the Flux_earth_ceres class.
 */

#ifndef SGNL_FLUX_EARTH_CERES_H
#define SGNL_FLUX_EARTH_CERES_H

#include "Flux.h"

/*!
 * @class Flux_earth_ceres
 * @date 20 May 2016
 * @author Zhen Li
 * @brief Calculates magnitude (in W/m^2) and direction of earth flux based on
 *        CERES model.
 */
class Flux_earth_ceres : public Flux
{
  public:
    struct triGridFlux {
        // Cartesian vertex[3];
        Cartesian centroid;
        double area;
        double longwave;
        double shortwave;
        // int children[4];
        // int parent;
        // int number;
        // int level;
    };

    Flux_earth_ceres() = default;

    void add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num) override;

    void compute_flux() override;

  private:
    static size_t m_level;
    static constexpr double m_Ra = 6371.0;       // unit: km
    static constexpr double m_Ra2 = m_Ra * m_Ra; // unit: km^2

    // 12 months data for all the level
    static std::vector<std::vector<std::vector<triGridFlux>>> m_ERPgrid;

    void visible_brute_force(Cartesian pos, std::vector<triGridFlux> &alltri);
    void add_triangle_flux(const triGridFlux &tri);

    // static Cartesian get_centroid(triGridFlux &tri);
    static Cartesian get_centroid(Cartesian vert0, Cartesian vert1,
                                  Cartesian vert2);

    // double great_circle_distance(double lat1, double lon1, double lat2,
    //                              double lon2, double radius) const;

    // bool IsPointInTri_sph(GVertex p[3], double lat, double lon);

    // bool visibility_test(triGridFlux &tri, Cartesian pos);

    // bool tri_circle_intersection(double r, double x[3], double y[3]);
    // bool visibility_test_new(triGridFlux &tri, Cartesian &subpos, double rv);

    // void visible_area(Cartesian pos,
    //                   std::vector<std::vector<triGridFlux>> &alltri,
    //                   std::vector<triGridFlux> &myres);

    static std::vector<std::vector<std::vector<triGridFlux>>>
    populateEMData(std::string datadir, size_t level);

    static void reading_tri_grid_file(std::string filename,
                                      std::vector<triGridFlux> &mydata);
};

#endif
