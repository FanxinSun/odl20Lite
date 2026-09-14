/*! @file Force_drag_nrlmsise00.h
	@author UCL SGNL, after John Keeling (2021)
	@date 15 September 2026
	@brief UCL ODL implementation of drag force model using NRLMSISE-00
 */

#ifndef SGNL_FORCE_DRAG_NRLMSISE00_H
#define SGNL_FORCE_DRAG_NRLMSISE00_H

#include "Force_drag.h"

#include <map>

/*!
 * @class Force_drag_nrlmsise00
 * @date 15 September 2026
 * @brief Thermosphere mass density from the NRLMSISE-00 empirical model.
 *
 * Unlike Force_drag_tiegcm this reads no density grid, so it has no window in
 * time, altitude or latitude to fall outside of. It is driven instead by solar
 * and geomagnetic indices, read from res/SW-All.csv (CelesTrak). If that file
 * is missing or does not cover the epoch, the model still runs on the quiet-Sun
 * defaults the NRLMSISE-00 documentation gives (F10.7 = 150, Ap = 4) and says
 * so once.
 *
 * The model itself is the reference C implementation in external/nrlmsise00.
 */
class Force_drag_nrlmsise00 : public Force_drag
{
  public:
    Force_drag_nrlmsise00() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

    /*!
     * @fn get_density(double mjd_utc, double alt, double lat, double lon)
     * @brief Total mass density in kg/m^3 at a time and geodetic position.
     * @param mjd_utc Modified Julian Date, UTC.
     * @param alt     Geodetic altitude in km.
     * @param lat     Geodetic latitude in radians.
     * @param lon     Geodetic longitude in radians.
     */
    double get_density(double mjd_utc, double alt, double lat, double lon);

  private:
    //! The three indices NRLMSISE-00 is driven by, for one day.
    struct Space_weather {
        double f107 = 150.0;  //!< observed F10.7 of the *previous* day
        double f107a = 150.0; //!< 81-day average centred on the day
        double ap = 4.0;      //!< daily magnetic index
        bool measured = false;
    };

    //! Indices by integer MJD, loaded once from res/SW-All.csv.
    static const std::map<long int, Space_weather> &space_weather();

    //! Indices for a day, falling back to the documented defaults.
    Space_weather weather_for(long int mjdn);

    //! Calendar date and day-of-year for an integer MJD.
    static void civil_from_mjdn(long int mjdn, int &year, int &month, int &day,
                                int &doy);
    static long int mjdn_from_civil(int year, int month, int day);

    //! Set once a day's indices are missing, so a long run reports it once.
    bool reported_default_weather = false;
};

#endif
