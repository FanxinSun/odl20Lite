/*! @file Flux_earth_ceres.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux_earth_ceres class.
 */

#include "../include/Flux_earth_ceres.h"

size_t Flux_earth_ceres::m_level = 0;

// 12 months data for all the level
std::vector<std::vector<std::vector<Flux_earth_ceres::triGridFlux>>>
    Flux_earth_ceres::m_ERPgrid = populateEMData("../res/earth_radiation", 4);

void Flux_earth_ceres::add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num)
{
    eci_flux_num += 0;
    ecef_flux_num += 4u << (m_level + m_level); // 4 * 4^m_level
}

void Flux_earth_ceres::compute_flux()
{
    tm UTC_cal = state->ecef.epoch.get_UTC_cal();
    size_t month = static_cast<size_t>(UTC_cal.tm_mon);

    // add fluxes for the visible triangles for the satellite
    visible_brute_force(state->ecef_rso, m_ERPgrid[month][m_level]);
}

// Test the highest level triangles one by one
void Flux_earth_ceres::visible_brute_force(Cartesian pos,
                                           std::vector<triGridFlux> &alltri)
{
    for (const auto &tri : alltri) {
        if (dot_product(pos, tri.centroid) > m_Ra2) {
            add_triangle_flux(tri);
        }
    }
}

// Calculates flux from a single triangle and pushes it to state->ecef_fluxes
void Flux_earth_ceres::add_triangle_flux(const triGridFlux &tri)
{
    Fluxstruct ecef_flux;

    ecef_flux.dir = state->ecef_rso - tri.centroid;
    double r_minus_p_mag2 = ecef_flux.dir.length2();
    double r_minus_p_mag = std::sqrt(r_minus_p_mag2);
    ecef_flux.dir /= r_minus_p_mag;

    // Merged cos_theta and coef calculations for efficiency:
    double coef = tri.area * (state->r2 - r_minus_p_mag2 - m_Ra2) /
                  (sgnlOPS::D_2_PI * m_Ra * r_minus_p_mag * r_minus_p_mag2);

    ecef_flux.mag_lw = tri.longwave * coef;

    // Set the shortwave part value according to the position of sun
    if (dot_product(state->ecef_sun, tri.centroid) > m_Ra2) {
        // There should be some additional scaling here based on angle to
        // sun, but not simply multiplying by sin_sun_elev/m_Ra.
        // Cosine of angle between longitudes of sun and point perhaps.
        ecef_flux.mag_sw = tri.shortwave * coef;
    }

    state->ecef_fluxes.push_back(ecef_flux);
}

/*
 1 is for start point and 2 is for end point

 the range of longitude is -PI to PI
 */
// double Flux_earth_ceres::great_circle_distance(double lat1, double lon1,
//                                                double lat2, double lon2,
//                                                double radius) const
// {
//     double delLon = fabs(lon2 - lon1);
//     if (fabs(delLon) >= 180.0) {
//         delLon = 360 - delLon;
//     }
//
//     // the same point
//     if (fabs(lat1 - lat2) < 1.0E-6 && fabs(lon1 - lon2) < 1.0E-6) {
//         return 0.0;
//     }
//     //lon1 = lon1 - 180.0;
//     //lon2 = lon2 - 180.0;
//
//     double de2ra = sgnlOPS::D_DEG2RAD;
//     lat1 *= de2ra;
//     lon1 *= de2ra;
//     lat2 *= de2ra;
//     lon2 *= de2ra;
//     delLon *= de2ra;
//
//     double d = 0.0;
//
//     double cosD = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(delLon);
//     d = (radius * acos(cosD));
//
//     return d;
// }

// /*treat it as sphere*/
// bool Flux_earth_ceres::IsPointInTri_sph(GVertex p[3], double lat, double lon)
// {
//     double pi = sgnlOPS::D_PI;
//     double deg2rad = sgnlOPS::D_DEG2RAD;

//     bool test = false;
//     //transfer lat and lon to xyz
//     double xyz[3] = {0.0};
//     //double a = 6408137;     // a = 6378137 for WGS84
//     //double b = 6386651.7;   // b=6356752.3142 for WGS84
//     //double m_radius = 6371000;
//     double eps = 1.0E-10;

//     double blh[3] = {lat * deg2rad, lon * deg2rad, 0.0};
//     //p[0].blh2xyz(blh, xyz, 0.0, m_radius);
//     //double m_Ra = 1.0;
//     xyz[2] = sin(blh[0]) * m_Ra;
//     xyz[1] = cos(blh[0]) * sin(blh[1]) * m_Ra;
//     xyz[0] = cos(blh[0]) * cos(blh[1]) * m_Ra;

//     double len = sqrt(pow(xyz[0], 2.0) + pow(xyz[1], 2.0) + pow(xyz[2], 2.0));
//     xyz[0] = xyz[0] / len;
//     xyz[1] = xyz[1] / len;
//     xyz[2] = xyz[2] / len;

//     double v[3][3] = {{0.0}};
//     for (int i = 0; i < 3; i++) {
//         double len = sqrt(pow(p[i].getX(), 2.0) + pow(p[i].getY(), 2.0) +
//                           pow(p[i].getZ(), 2.0));
//         v[i][0] = p[i].getX() / len;
//         v[i][1] = p[i].getY() / len;
//         v[i][2] = p[i].getZ() / len;
//     }

//     double n[3][3] = {{0.0}};
//     for (int i = 0; i < 3; i++) {
//         int j = i + 1;
//         if (j == 3) {
//             j = 0;
//         }
//         n[i][0] = v[i][1] * v[j][2] - v[i][2] * v[j][1];
//         n[i][1] = v[i][2] * v[j][0] - v[j][2] * v[i][0];
//         n[i][2] = v[i][0] * v[j][1] - v[i][1] * v[j][0];

//         double sum =
//             sqrt(n[i][0] * n[i][0] + n[i][1] * n[i][1] + n[i][2] * n[i][2]);
//         n[i][0] = n[i][0] / sum;
//         n[i][1] = n[i][1] / sum;
//         n[i][2] = n[i][2] / sum;
//     }

//     double test1 = xyz[0] * n[0][0] + xyz[1] * n[0][1] + xyz[2] * n[0][2];
//     double test2 = xyz[0] * n[1][0] + xyz[1] * n[1][1] + xyz[2] * n[1][2];
//     double test3 = xyz[0] * n[2][0] + xyz[1] * n[2][1] + xyz[2] * n[2][2];

//     bool t1 = (test1 > 0 || fabs(test1) < eps);
//     bool t2 = (test2 > 0 || fabs(test2) < eps);
//     bool t3 = (test3 > 0 || fabs(test3) < eps);

//     test = t1 && t2 && t3;

//     return test;
// }

//get the centroid of this triangle
// Cartesian Flux_earth_ceres::get_centroid(triGridFlux &tri)
// {
//     Cartesian p = tri.vertex[0] + tri.vertex[1] + tri.vertex[2];
//
//     //extend this point onto the sphere
//     p *= m_Ra / p.length();
//
//     return p;
// }

//get the centroid of this triangle
Cartesian Flux_earth_ceres::get_centroid(Cartesian vert0, Cartesian vert1,
                                         Cartesian vert2)
{
    Cartesian p = vert0 + vert1 + vert2;

    //extend this point onto the sphere
    p *= m_Ra / p.length();

    return p;
}

/*
 * a function to check whether a plane triangle and a circle
 * return : 0 for no intersection
 *          1 means either partly intersected or triangle is inside the circle
 */
// bool Flux_earth_ceres::tri_circle_intersection(double r, double x[3],
//                                                double y[3])
// {
//     double r2 = r * r;
//     double csqr[3] = {0.0};
//     //test1: vertices within circle
//     for (int i = 0; i < 3; i++) {
//         csqr[i] = x[i] * x[i] + y[i] * y[i] - r2;
//         if (csqr[i] <= 0.0) {
//             return true;
//         }
//     }
//
//     //test2: circle centre within triangle
//     double ex[3] = {0.0}, ey[3] = {0.0};
//
//     ex[0] = x[1] - x[0];
//     ey[0] = y[1] - y[0];
//
//     ex[1] = x[2] - x[1];
//     ey[1] = y[2] - y[1];
//
//     ex[2] = x[0] - x[2];
//     ey[2] = y[0] - y[2];
//
//     if (ey[0] * x[0] >= ex[0] * y[0] && ey[1] * x[1] >= ex[1] * y[1] &&
//         ey[2] * x[2] >= ex[2] * y[2]) {
//         return true;
//     }
//
//     //test3: circle intersects edge
//
//     for (int i = 0; i < 3; i++) {
//         double k = x[i] * ex[i] + y[i] * ey[i];
//
//         if (k >= 0.0) {
//             double len = ex[i] * ex[i] + ey[i] * ey[i];
//             if (k <= len) {
//                 if (csqr[i] * len <= k * k) {
//                     return true;
//                 }
//             }
//         }
//     }
//
//     return false;
// }

/*
 * the new method for testing the visibility of a triangle
 *
 */
// bool Flux_earth_ceres::visibility_test_new(triGridFlux &tri, Cartesian &subpos,
//                                            double rv)
// {
//     double x[3] = {0.0}, y[3] = {0.0};
//     Cartesian normal;
//     // double points[2][3] = {{0.0}};
//     normal = normalise(subpos);
//     //t = dot_product(normal,subpos);
//     bool vertex_in_hemisphere[3] = {false};
//     //exclude the triangle that is not at the same semi-sphere as subpos
//     for (int i = 0; i < 3; i++) {
//         if (dot_product(tri.vertex[i], normal) >= 0.0) {
//             vertex_in_hemisphere[i] = true;
//         } else {
//             vertex_in_hemisphere[i] = false;
//         }
//     }
//
//     if (vertex_in_hemisphere[0] == false && vertex_in_hemisphere[1] == false &&
//         vertex_in_hemisphere[2] == false) {
//         return false;
//     }
//
//     Cartesian x_t, y_t;
//     if (normal.x != 0.0) {
//         x_t.set(m_Ra / normal.x, 0.0, 0.0);
//     } else if (normal.y != 0.0) {
//         x_t.set(0.0, m_Ra / normal.y, 0.0);
//     } else if (normal.z != 0.0) {
//         x_t.set(0.0, 0.0, m_Ra / normal.z);
//     }
//
//     x_t = x_t - subpos;
//     x_t.normalise();
//     y_t = cross_product(normal, x_t);
//
//     //here, choose a special projection, just ignore the z cooridinate
//     // !!! pay attention to the projection transformation !!!
//     for (int i = 0; i < 3; i++) {
//         double t = dot_product((subpos - tri.vertex[i]), normal);
//         Cartesian t1 = normal * t + tri.vertex[i] - subpos;
//
//         x[i] = dot_product(t1, x_t);
//         y[i] = dot_product(t1, y_t);
//     }
//
//     return tri_circle_intersection(rv, x, y);
// }

// /*
//  * visiability test for one triangle
//  *
//  * test the 3 points of the triangle, if any one of them is visible, return true
//  */
// bool Flux_earth_ceres::visibility_test(GTriangle<GVertex> &tri, Cartesian pos)
// {
//     double pi = sgnlOPS::D_PI;
//     double deg2rad = sgnlOPS::D_DEG2RAD;
//     double rad2deg = sgnlOPS::D_RAD2DEG;

//     bool test = false;
//     double m[9] = {0.0};

//     // the neu coordinate of satellite relative to the station
//     double neu_sat[3] = {0.0};

//     // the difference in coordinate between satellite and station
//     double tmp[3] = {0.0};

//     // azimuth and elevation of the satellite
//     double azel[2] = {0.0};

//     GVertex tpoints[3];
//     tpoints[0] = tri.getPoint(0);
//     tpoints[1] = tri.getPoint(1);
//     tpoints[2] = tri.getPoint(2);

//     // get the sub-satellite point first
//     double len = sqrt(pos.x * pos.x + pos.y * pos.y);
//     double subsatpoint[2] = {0.0};                      //substellar
//     subsatpoint[0] = atan(pos.z / sqrt(len)) * rad2deg; // latitude
//     subsatpoint[1] = atan2(pos.y, pos.x) * rad2deg;     // longitude
//     if (subsatpoint[1] < 0.0) {
//         subsatpoint[1] = subsatpoint[1] + 360.0;
//     }

//     bool test1 = IsPointInTri_sph(tpoints, subsatpoint[0], subsatpoint[1]);
//     // if the subsatpoint is inside the triangle, that means this triangle must be visible
//     if (test1) {
//         test = true;
//     } else {
//         // 如果不在，则距离星下点最近的点的高度角最大，因为星下点高度角是90度,还要考虑到三角形边上的点
//         int minIndex1 = 0;

//         //至少40000km 最下的距离意味着最大的高度角
//         double minDistance1 = 9999000;
//         //至少40000km 最下的距离意味着最大的高度角
//         double minDistance2 = 9999000;

//         // the longtitude and latitude of the nearest point
//         double nearestPointBL[2] = {0.0};

//         // the xyz coordinates of the nearest point
//         double neareatPointXYZ[3] = {0.0};

//         for (int i = 0; i < 3; i++) {
//             double dis = great_circle_distance(tpoints[i].getLat(),
//                                                tpoints[i].getLon(),
//                                                subsatpoint[0],
//                                                subsatpoint[1], m_Ra);
//             if (dis < minDistance1) {
//                 minIndex1 = i;
//                 minDistance1 = dis;
//             }
//         }

//         //找到了三角形3个顶点中离星下点最近的点
//         //然后寻找三条边上离星下点最近的点
//         // first, find out the direction vector of the intersection line
//         // all these planes should include the origin of the sphere
//         for (int j = 0; j < 3; j++) {
//             int k = j + 1;
//             if (k == 3) {
//                 k = 0;
//             }

//             double a[3] = {0.0}; // the vector of substellar point to the origin
//             double b[3] = {0.0}; // the normal vector of the known edge plane
//             double c[3] = {0.0}; // the normal vecor of the new plane
//             double d[3] = {0.0}; // the director vector of the intersection line

//             double t1[3] = {0.0}; // the vector of the point A and origin
//             double t2[3] = {0.0}; // the vector of the point B and origin
//             t1[0] = tpoints[j].getX();
//             t1[1] = tpoints[j].getY();
//             t1[2] = tpoints[j].getZ();
//             t2[0] = tpoints[k].getX();
//             t2[1] = tpoints[k].getY();
//             t2[2] = tpoints[k].getZ();

//             a[0] = pos.x;
//             a[1] = pos.y;
//             a[2] = pos.z;

//             double t1_len = sqrt(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
//             double t2_len = sqrt(t2[0] * t2[0] + t2[1] * t2[1] + t2[2] * t2[2]);
//             double a_len = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);

//             a[0] = a[0] / a_len;
//             a[1] = a[1] / a_len;
//             a[2] = a[2] / a_len;

//             t1[0] = t1[0] / t1_len;
//             t1[1] = t1[1] / t1_len;
//             t1[2] = t1[2] / t1_len;

//             t2[0] = t2[0] / t2_len;
//             t2[1] = t2[1] / t2_len;
//             t2[2] = t2[2] / t2_len;

//             b[0] = t1[1] * t2[2] - t1[2] * t2[1];
//             b[1] = t1[2] * t2[0] - t1[0] * t2[2];
//             b[2] = t1[0] * t2[1] - t1[1] * t2[0];

//             c[0] = a[1] * b[2] - a[2] * b[1];
//             c[1] = a[2] * b[0] - a[0] * b[2];
//             c[2] = a[0] * b[1] - a[1] * b[0];

//             d[0] = c[1] * b[2] - c[2] * b[1];
//             d[1] = c[2] * b[0] - c[0] * b[2];
//             d[2] = c[0] * b[1] - c[1] * b[0];

//             //the vector d should be between vector t1 and t2
//             // this can help to determine the direction of the intersection line
//             double len1 = sqrt(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
//             double len2 = sqrt(t2[0] * t2[0] + t2[1] * t2[1] + t2[2] * t2[2]);
//             double theta1 =
//                 acos((t1[0] * t2[0] + t1[1] * t2[1] + t1[2] * t2[2]) /
//                      (len1 * len2));
//             double len3 = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
//             double theta2 = acos((t1[0] * d[0] + t1[1] * d[1] + t1[2] * d[2]) /
//                                  (len1 * len3));
//             double theta3 = acos((t2[0] * d[0] + t2[1] * d[1] + t2[2] * d[2]) /
//                                  (len2 * len3));
//             if (theta2 > theta1 || theta3 > theta1) {
//                 d[0] = -d[0];
//                 d[1] = -d[1];
//                 d[2] = -d[2];
//             }

//             len1 = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
//             double theta =
//                 acos((a[0] * d[0] + a[1] * d[1] + a[2] * d[2]) / (len1 * len3));
//             double dis = theta * m_Ra;

//             //check whether this point is inside the triangle
//             double Ra_over_len3 = m_Ra / len3;
//             neareatPointXYZ[0] = d[0] * Ra_over_len3;
//             neareatPointXYZ[1] = d[1] * Ra_over_len3;
//             neareatPointXYZ[2] = d[2] * Ra_over_len3;
//             len = sqrt(neareatPointXYZ[0] * neareatPointXYZ[0] +
//                        neareatPointXYZ[1] * neareatPointXYZ[1]);
//             nearestPointBL[0] =
//                 atan(neareatPointXYZ[2] / sqrt(len)) * rad2deg; // latitude
//             nearestPointBL[1] = atan2(neareatPointXYZ[1], neareatPointXYZ[0]) *
//                                 rad2deg; // longitude
//             if (nearestPointBL[1] < 0.0) {
//                 nearestPointBL[1] = nearestPointBL[1] + 360.0;
//             }
//             bool t =
//                 IsPointInTri_sph(tpoints, nearestPointBL[0], nearestPointBL[1]);
//             if (t == false) {
//                 dis = 9999000;
//             }

//             if (dis < minDistance2) {
//                 minDistance2 = dis;
//                 double Ra_over_len2 = m_Ra / len2;
//                 neareatPointXYZ[0] = d[0] * Ra_over_len2;
//                 neareatPointXYZ[1] = d[1] * Ra_over_len2;
//                 neareatPointXYZ[2] = d[2] * Ra_over_len2;
//             }
//         }

//         if (minDistance2 < minDistance1) {
//             len = sqrt(neareatPointXYZ[0] * neareatPointXYZ[0] +
//                        neareatPointXYZ[1] * neareatPointXYZ[1]);
//             nearestPointBL[0] =
//                 atan(neareatPointXYZ[2] / sqrt(len)) * rad2deg; // latitude
//             nearestPointBL[1] = atan2(neareatPointXYZ[1], neareatPointXYZ[0]) *
//                                 rad2deg; // longitude
//             if (nearestPointBL[1] < 0.0) {
//                 nearestPointBL[1] = nearestPointBL[1] + 360.0;
//             }
//         } else {
//             neareatPointXYZ[0] = tpoints[minIndex1].getX();
//             neareatPointXYZ[1] = tpoints[minIndex1].getY();
//             neareatPointXYZ[2] = tpoints[minIndex1].getZ();
//             nearestPointBL[0] = tpoints[minIndex1].getLat();
//             nearestPointBL[1] = tpoints[minIndex1].getLon();
//         }

//         //start calculate the elevation
//         double tmp[3] = {0.0};
//         double L = nearestPointBL[1] * deg2rad;
//         double B = nearestPointBL[0] * deg2rad;

//         double rp = pos.x * neareatPointXYZ[0] + pos.y * neareatPointXYZ[1] +
//                     pos.z * neareatPointXYZ[2];

//         if (rp >= m_Ra2) {
//             test = true;
//         } else {
//             test = false;
//         }
//     }

//     return test;
// }

/*
 * ********the searching strategy which is regarded faster******
 * According to the position of satellite, find out which triangles are visible.
 * inputs: pos, alltri
 * output: triCode, the code string of the visiable triangles
 */
// void Flux_earth_ceres::visible_area(
//     Cartesian pos, std::vector<std::vector<triGridFlux>> &alltri,
//     std::vector<triGridFlux> &myres)
// {
//     // the maxlevel of triangle division
//     int maxlevel = static_cast<int>(alltri.size());
//     int levelID = 0; // start with level 0
//
//     std::vector<triGridFlux> mytri;
//
//     double r = pos.length();
//     Cartesian subpos = pos;
//     subpos.normalise();
//     subpos = subpos * m_Ra;
//     // the radius of visible circle area
//     double rv = m_Ra * sqrt(r * r - m_Ra * m_Ra) / (r + m_Ra);
//
//     // int testcount = 0;
//
//     while (1) {
//
//         if (levelID == 0) {
//             // the 8 triangles in level 0
//             for (int k = 0; k < 8; k++) {
//                 //bool t = visibility_test(alltri[levelID][k], pos);
//                 bool t = visibility_test_new(alltri[levelID][k], subpos, rv);
//                 if (t == true) {
//                     myres.push_back(alltri[levelID][k]);
//                 }
//                 // testcount++;
//             }
//         } else {
//             //every triangle has 4 children
//             for (const auto &tri : mytri) {
//                 //std::vector<GString> strcode;
//                 //tri.getCtag(strcode); // get str code of the children
//
//                 for (int j = 0; j < 4; j++) // check its 4 children
//                 {
//                     int lev = tri.level + 1;
//                     int num = tri.children[j] - 1;
//                     //bool t = visibility_test(alltri[level][num - 1], pos);
//                     bool t = visibility_test_new(alltri[lev][num], subpos, rv);
//                     if (t == true) {
//                         myres.push_back(alltri[lev][num]);
//                     }
//                 }
//                 // testcount++;
//             }
//         }
//
//         levelID++;
//         if (levelID >= maxlevel) {
//             break;
//         }
//         mytri = myres;
//         myres.clear();
//     }
//
//     //printf("testCOUNT:::******:%d\n",testcount );
// }

std::vector<std::vector<std::vector<Flux_earth_ceres::triGridFlux>>>
Flux_earth_ceres::populateEMData(std::string datadir, size_t level)
{
    std::vector<std::vector<std::vector<triGridFlux>>> ERPgrid;

    m_level = level;

    ERPgrid.resize(12);

    for (size_t i = 0; i < 12; i++) {
        ERPgrid[i].resize(level + 1);

        for (size_t j = 0; j < level + 1; j++) {
            std::stringstream filepath;
            filepath << datadir << "/" << std::setfill('0') << std::setw(2)
                     << (i + 1) << "/level" << j << ".txt";

            reading_tri_grid_file(filepath.str(), ERPgrid[i][j]);
        }
    }

    return ERPgrid;
}

void Flux_earth_ceres::reading_tri_grid_file(std::string filename,
                                             std::vector<triGridFlux> &mydata)
{
    std::vector<triGridFlux> test;

    std::ifstream fs(filename);
    if (!fs.good()) {
        std::cerr << "Flux_earth_ceres, opening file: " << filename
                  << " failed!" << std::endl;
        std::exit(1);
    }

    std::string tmpstr;
    size_t pos, pos2;

    while (!fs.eof()) {
        //char tmpstr[1024]={0};
        getline(fs, tmpstr);
        if (tmpstr == "") {
            break;
        }
        std::vector<std::string> split_str;

        // Strip whitespace from left and right first:
        tmpstr = tmpstr.substr(0, tmpstr.find_last_not_of(" ") + 1);
        tmpstr = tmpstr.substr(tmpstr.find_first_not_of(" "));

        pos = tmpstr.find(" ");
        pos2 = tmpstr.find_first_not_of(" ", pos);

        while (pos < std::string::npos) {
            split_str.push_back(tmpstr.substr(0, pos));
            tmpstr = tmpstr.substr(pos2);
            pos = tmpstr.find(" ");
            pos2 = tmpstr.find_first_not_of(" ", pos);
        }

        split_str.push_back(tmpstr);

        // std::vector<GString> split_str = tmpstr.split();

        if (split_str.size() == 14) // the last level
        {
            triGridFlux mytri;
            // mytri.level = std::stoi(split_str[0].substr(0, 1));
            // mytri.number = std::stoi(split_str[0].substr(1));
            // mytri.parent = std::stoi(split_str[1].substr(1));
            //without children

            Cartesian vert0, vert1, vert2;

            vert0.x = std::stod(split_str[2]) / 1000.0;
            vert0.y = std::stod(split_str[3]) / 1000.0;
            vert0.z = std::stod(split_str[4]) / 1000.0;
            // mytri.vertex[0] = vert0;

            vert1.x = std::stod(split_str[5]) / 1000.0;
            vert1.y = std::stod(split_str[6]) / 1000.0;
            vert1.z = std::stod(split_str[7]) / 1000.0;
            // mytri.vertex[1] = vert1;

            vert2.x = std::stod(split_str[8]) / 1000.0;
            vert2.y = std::stod(split_str[9]) / 1000.0;
            vert2.z = std::stod(split_str[10]) / 1000.0;
            // mytri.vertex[2] = vert2;

            mytri.area = std::stod(split_str[11]); //km^2

            mytri.longwave = std::stod(split_str[12]);
            mytri.shortwave = std::stod(split_str[13]);

            mytri.centroid = get_centroid(vert0, vert1, vert2);

            mydata.push_back(mytri);

        } else if (split_str.size() == 18) // the normal level
        {
            triGridFlux mytri;
            // mytri.level = std::stoi(split_str[0].substr(0, 1));
            // mytri.number = std::stoi(split_str[0].substr(1));
            // mytri.parent = std::stoi(split_str[1].substr(1));

            // mytri.children[0] = std::stoi(split_str[2]);
            // mytri.children[1] = std::stoi(split_str[3]);
            // mytri.children[2] = std::stoi(split_str[4]);
            // mytri.children[3] = std::stoi(split_str[5]);

            Cartesian vert0, vert1, vert2;

            vert0.x = std::stod(split_str[6]) / 1000.0;
            vert0.y = std::stod(split_str[7]) / 1000.0;
            vert0.z = std::stod(split_str[8]) / 1000.0;
            // mytri.vertex[0] = vert0;

            vert1.x = std::stod(split_str[9]) / 1000.0;
            vert1.y = std::stod(split_str[10]) / 1000.0;
            vert1.z = std::stod(split_str[11]) / 1000.0;
            // mytri.vertex[1] = vert1;

            vert2.x = std::stod(split_str[12]) / 1000.0;
            vert2.y = std::stod(split_str[13]) / 1000.0;
            vert2.z = std::stod(split_str[14]) / 1000.0;
            // mytri.vertex[2] = vert2;

            mytri.area = std::stod(split_str[15]); //km^2

            mytri.longwave = std::stod(split_str[16]);
            mytri.shortwave = std::stod(split_str[17]);

            mytri.centroid = get_centroid(vert0, vert1, vert2);

            mydata.push_back(mytri);
        } else {
            std::cerr << "Flux_earth_ceres, format error in ERP data file: "
                      << filename << std::endl;
            std::exit(1);
        }
    }
}
