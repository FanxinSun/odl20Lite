/*! @file Eclipse_model.cpp
	@author David Harrison
	@date 18 January 2017
	@brief SGNL OPS file defining the Eclipse_model class.
 */

#include "../include/Eclipse_model.h"

/*
 * @author David Harrison
 * @date 7 December 2016
 * <Adhya04> Sima Adhya, Anthony Sibthorpe, Marek Ziebart and Paul Cross
 *     "Oblate Earth Eclipse State Algorithm for Low-Earth-OrbitingSatellites",
 *     Journal of Spacecraft and Rockets, Vol. 41, No. 1 (2004), pp. 157-159.
 *     doi: 10.2514/1.1485
 *
 * Two versions of this function have been written before, one in 2002 by Sima
 * Adhya and Ant Sibthorpe, and another in March 2015 by Santosh Bhattarai.
 *
 * The method used in this function differs slightly from that presented in the
 * paper cited. here are the key differences:
 * 1) All of the vectors used are in the ECEF frame, not ECI.
 * 2) The sun vector points to the sun from the spacecraft position.
 * 3) The calculations performed have been corrected, simplified and optimised.
 * 4) A more accurate method of finding sun edges is used, instead of sun_perp.
 * 5) A penumbral flux scaling method has been added.
 *
 * This function determines eclipse state based on an (oblate) spheroidal Earth
 * model. Returns double meaning; 1.0 full phase, 0.0 umbra, penumbra otherwise.
 */
double Eclipse_model::eclipse(double rso_sun_distance, double eci_sun_distance,
                              Cartesian rso, Cartesian sun)
{
    constexpr double R2 = sgnlOPS::solar_radius * sgnlOPS::solar_radius;

    double eclipse_state = 1.0;

    // Only run eclipse check if spacecraft is further from sun than Earth is.
    if (rso_sun_distance > eci_sun_distance) {
        double sTs = sun.length2();
        double sTr = dot_product(sun, rso);
        double rTr = rso.length2();

        // Use this to solve for the linear combination of rso and sun vectors
        // that give the sun edge vectors.
        double D = std::sqrt((sTs - R2) * R2 / (sTs * rTr - sTr * sTr));
        Cartesian Drso = D * rso;

        // Create the sun edge vector closest to Earth.
        Cartesian sun_edge1 = (1.0 + (sTr * D - R2) / sTs) * sun - Drso;

        if (sun_edge_earth_intersection(rso, sun_edge1) > 0.0) {

            // Create the sun edge vector furthest from Earth.
            Cartesian sun_edge2 = (1.0 - (sTr * D + R2) / sTs) * sun + Drso;

            if (sun_edge_earth_intersection(rso, sun_edge2) >= 0.0) {
                // Both sun edges are blocked, so no flux.
                eclipse_state = 0.0;
            } else {
                // Find the Earth edge vector that lies between the sun edges
                Cartesian earth_edge = get_earth_edge(rso, sun);

                // Only one edge of the sun is blocked, find the penumbral flux.
                // eclipse_state =
                //     penumbra_flux_scale(sun_edge1, earth_edge, sun_edge2);
                eclipse_state =
                    penumbra_flux_scale(rso, earth_edge, sun, sun_edge1);
            }
        }
    }

    return eclipse_state;
}

// Returns the discriminant of a quadratic equation that determines whether a
// vector b intersects the Earth (using an ellipsoidal model).
// If the return value is positive the vector b passes through the Earth.
// If it is negative then vector b does not pass through the Earth.
// If it is exactly zero then vector b is a tangent to the surface (unlikely).
inline double Eclipse_model::sun_edge_earth_intersection(Cartesian rso,
                                                         Cartesian b)
{
    constexpr double a2 = sgnlOPS::wgs84_equatorial_radius2;
    constexpr double a_b2 = sgnlOPS::wgs84_a_b2;

    // Matrix A = [[1,0,0],[0,1,0],[0,0,(a/b)^2]]
    double rTAr = (rso.x * rso.x + rso.y * rso.y) + (rso.z * rso.z) * a_b2;
    double rTAb = (rso.x * b.x + rso.y * b.y) + (rso.z * b.z) * a_b2;
    double bTAb = (b.x * b.x + b.y * b.y) + (b.z * b.z) * a_b2;

    return (rTAb * rTAb - bTAb * (rTAr - a2));
}

// Returns a vector from the rso that satisfies 3 conditions:
// 1) vector is tangential to the surface of the earth ellipsoid
// 2) vector is in the same plane as the earth and sun vectors
// 3) since there are 2 solutions to above, return the one closest to the sun
// Conditions 2 and 3 are easily satisfied as the solution is made from a
// linear combination of the rso and sun vectors.
inline Cartesian Eclipse_model::get_earth_edge(Cartesian rso, Cartesian sun)
{
    constexpr double a2 = sgnlOPS::wgs84_equatorial_radius2;
    constexpr double a_b2 = sgnlOPS::wgs84_a_b2;

    // Matrix A = [[1,0,0],[0,1,0],[0,0,(a/b)^2]]
    double rTAr = (rso.x * rso.x + rso.y * rso.y) + (rso.z * rso.z) * a_b2;
    double rTAs = (rso.x * sun.x + rso.y * sun.y) + (rso.z * sun.z) * a_b2;
    double sTAs = (sun.x * sun.x + sun.y * sun.y) + (sun.z * sun.z) * a_b2;

    // Use this to solve for the linear combination of rso and sun vectors
    // that give the Earth edge vectors.
    double D = std::sqrt((rTAr - a2) * a2 / (rTAr * sTAs - rTAs * rTAs));

    // Earth edge closest to sun
    Cartesian edge = ((a2 - rTAs * D) / rTAr - 1.0) * rso + D * sun;

    // Testing:
    // This is the second edge, always further away from the sun:
    // Cartesian edge2 = ((a2 + rTAs * D) / rTAr - 1.0) * rso - D * sun;

    // double inter1 = sun_edge_earth_intersection(rso, edge);
    // double inter2 = sun_edge_earth_intersection(rso, edge2);
    // std::cout << "intersection1: " << inter1 << std::endl;
    // std::cout << "intersection2: " << inter2 << std::endl;
    // std::cout << "------------------------------" << std::endl;

    return edge;
}

// Adapted from conical shadow function from Montenbruck 3.4.2, pages 82 - 83
// Angles a, b and c are calculated differently.
// In particular angle b is calculated based on the Earth "radius" at the point
// of eclipse from the WGS84 ellipsoidal model.
double Eclipse_model::penumbra_flux_scale(Cartesian rso, Cartesian earth_edge,
                                          Cartesian sun, Cartesian sun_edge)
{
    const double sun2 = sun.length2();
    const double rso2 = rso.length2();

    const double a = std::acos(dot_product(sun_edge, sun) /
                               std::sqrt(sun_edge.length2() * sun2));
    const double b = std::acos(-dot_product(earth_edge, rso) /
                               std::sqrt(earth_edge.length2() * rso2));
    const double c = std::acos(-dot_product(rso, sun) / std::sqrt(rso2 * sun2));

    const double a2 = a * a;
    const double b2 = b * b;

    double factor;

    if (c >= a + b) {
        // No eclipse
        factor = 1.0;
    } else if (b - a >= c) {
        // Sun completely occluded
        factor = 0.0;
    } else if (a - b >= c) {
        // Annular eclipse, ie: sun is visible around the occluding body
        factor = 1.0 - b2 / a2;
    } else {
        double x = (c * c + a2 - b2) / (2.0 * c);
        double y = std::sqrt(a2 - x * x);
        double A = a2 * std::acos(x / a) + b2 * std::acos((c - x) / b) - c * y;
        factor = 1.0 - A / (sgnlOPS::D_PI * a2);
    }

    factor = std::min(factor, 1.0);
    factor = std::max(factor, 0.0);

    return factor;
}

// Since the Earth appears so much larger then the sun, even for GEO satellites,
// this function treats the Earth as a straight edge occluding the sun, and
// calculates the remaining relative visible area of the circular segment.
double Eclipse_model::penumbra_flux_scale(Cartesian blocked_edge,
                                          Cartesian earth_edge,
                                          Cartesian visible_edge)
{
    double norm2 = visible_edge.length2();
    double norm1 = std::sqrt(norm2 * earth_edge.length2());

    double cos_alpha1 = dot_product(visible_edge, earth_edge) / norm1;
    double cos_alpha2 = dot_product(visible_edge, blocked_edge) / norm2;

    // In case the visible edge and earth edge are very close
    if (cos_alpha1 >= 1.0) {
        return 0.0;
    }
    // In case the blocked edge and earth edge are very close
    else if (cos_alpha2 >= cos_alpha1) {
        return 1.0;
    } else {
        // Height of the visible circular segment:
        double h = 2.0 * std::acos(cos_alpha1) / std::acos(cos_alpha2);

        // theta is the angle of the visible circular segment
        // sin(theta)/2 and theta/2
        double sin_theta_2 = std::sqrt(h * (2.0 - h)) * (1.0 - h);
        double theta_2 = std::acos(1.0 - h);

        // Return relative area of circular segment still in sunlight
        return (theta_2 - sin_theta_2) / sgnlOPS::D_PI;
    }
}
