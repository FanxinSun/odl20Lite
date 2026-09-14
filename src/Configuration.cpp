/*! @file Configuration.cpp
	@author Santosh Bhattarai
	@date 13 January 2015
	@brief SGNL OPS header file defining the object Configuration, which parses
           and holds parameters from a config file.

	Each instance of a Configuration object will set the configurable parameters
	relating to the initial conditions (i.e. position, velocity, time, mass),
	the required prediction length, the propagator to be used, as well as all of
	parameters required by the force models.
 */

#include "../include/Configuration.h"

/**** Member functions ****/

Configuration::Configuration()
{
    set_defaults();
}

Configuration::Configuration(std::string filename)
{
    set_defaults();

    if (!parse_config_file(filename)) {
        std::cerr << "Error: configurable parameters file could not be loaded."
                  << std::endl
                  << std::endl;
    }

    set_initial_state();
}

Configuration::Configuration(std::string filename, std::string x0,
                             std::string y0, std::string z0, std::string u0,
                             std::string v0, std::string w0, std::string in_t0)
{
    set_defaults();

    if (!parse_config_file(filename)) {
        std::cerr << "Error: configurable parameters file could not be loaded."
                  << std::endl
                  << std::endl;
    }

    // Set ECI state based on arguments instead of config file
    initial_state.x = std::stold(x0);
    initial_state.y = std::stold(y0);
    initial_state.z = std::stold(z0);
    initial_state.u = std::stold(u0);
    initial_state.v = std::stold(v0);
    initial_state.w = std::stold(w0);

    t0 = in_t0;

    // Prevent any TLE input in config file overriding state passed by arguments
    tle0 = "";
    tle1 = "";
    tle2 = "";

    set_initial_state();
}

bool Configuration::parse_config_file(std::string filename)
{
    std::string line, param, value;
    const std::string delim = "=";
    const std::string whitespace = " \f\n\r\t\v";

    size_t pos;

    // Open configurable parameters file
    std::ifstream infile(filename);

    // Check that the file has opened successfully
    if (infile.fail() || !infile.is_open()) {
        return false;
    }

    // Load parameters from file
    while (getline(infile, line)) {

        pos = line.find(delim, 1);

        // Process line if parameter-value pair exists
        if (pos < line.length()) {
            param = line.substr(0, pos);
            value = line.substr(pos + 1);

            // Strip whitespace from left and right of each:
            param = param.substr(0, param.find_last_not_of(whitespace) + 1);
            param = param.substr(param.find_first_not_of(whitespace));

            value = value.substr(0, value.find_last_not_of(whitespace) + 1);
            value = value.substr(value.find_first_not_of(whitespace));

            // clang-format off
			if(param=="t0")              {              t0 = value;            }

			if(param=="tle0")            {            tle0 = value;            }
			if(param=="tle1")            {            tle1 = value;            }
			if(param=="tle2")            {            tle2 = value;            }

			if(param=="x0")              { initial_state.x = std::stold(value);}
			if(param=="y0")              { initial_state.y = std::stold(value);}
			if(param=="z0")              { initial_state.z = std::stold(value);}
			if(param=="u0")              { initial_state.u = std::stold(value);}
			if(param=="v0")              { initial_state.v = std::stold(value);}
			if(param=="w0")              { initial_state.w = std::stold(value);}

            if(param=="spacecraft")      {      spacecraft = value;            }
			if(param=="mass")            {            mass = std::stod(value); }
			if(param=="area")            {            area = std::stod(value); }
			if(param=="reflectivity")    {    reflectivity = std::stod(value); }
			if(param=="specularity")     {     specularity = std::stod(value); }
			if(param=="srp_scale")       {       srp_scale = std::stod(value); }
			if(param=="srp_scale_amp")   {   srp_scale_amp = std::stod(value); }
			if(param=="srp_scale_period"){srp_scale_period = std::stod(value); }

			if(param=="propagator")      {      propagator = std::stoi(value); }
			if(param=="step_size")       {       step_size = std::stod(value); }

			if(param=="simulation_time") { simulation_time = std::stod(value); }
            if(param=="output_interval") { output_interval = std::stod(value); }
            if(param=="output_format")   {   output_format = value;            }

            // Force models:
			if(param=="gravity_model")   {   gravity_model = std::stoi(value); }
			if(param=="grav_degree")     {     grav_degree = std::stoi(value); }
			if(param=="grav_order")      {      grav_order = std::stoi(value); }

            if(param=="magnetic_model")  {  magnetic_model = std::stoi(value); }
            if(param=="mag_degree")      {      mag_degree = std::stoi(value); }
            if(param=="mag_order")       {       mag_order = std::stoi(value); }
            if(param=="specific_charge") { specific_charge = std::stod(value); }

			if(param=="antenna_thrust")  {  antenna_thrust = std::stoi(value); }
			if(param=="drag")            {            drag = std::stoi(value); }
			if(param=="gr_correction")   {  gr_corrections = std::stoi(value); }
			if(param=="srp")             {             srp = std::stoi(value); }
			if(param=="erp")             {             erp = std::stoi(value); }
			if(param=="trr")             {             trr = std::stoi(value); }
			if(param=="rp_model")        {        rp_model = std::stoi(value); }
			if(param=="third_body")      {      third_body = std::stoi(value); }
			if(param=="y_bias")          {          y_bias = std::stoi(value); }
			if(param=="pole_tide")       {       pole_tide = std::stoi(value); }
			if(param=="solid_earth_tide"){solid_earth_tide = std::stoi(value); }
			if(param=="time_var_grav")   {   time_var_grav = std::stoi(value); }

			// TO-DO: the following two parameters are temporary fixes, remove
			// these when properly sorting out the approach to drag modeling
			// in the Orbit Prediction Software
			if(param=="location")        {        location = std::stoi(value); }
			if(param=="GMTtime")         {         GMTtime = std::stoi(value); }

            // clang-format on
        }
    }

    infile.close();

    // Make sure that Earth gravity is set correctly
    if (grav_degree < 0) {
        grav_degree = 0;
    }
    if (grav_order < 0 || grav_order > grav_degree) {
        grav_order = grav_degree;
    }

    // Make sure that Earth magnetic field is set correctly
    if (mag_degree < 0) {
        mag_degree = 0;
    }
    if (mag_order < 0 || mag_order > mag_degree) {
        mag_order = mag_degree;
    }
    if (magnetic_model == 0) {
        mag_degree = 0;
        mag_order = 0;
    }
    if (mag_degree == 0 || specific_charge == 0.0) {
        magnetic_model = 0;
    }

    if (srp != 1 && srp != 2) {
        srp = 0;
    }

    if (erp != 1 && erp != 2 && erp != 3) {
        erp = 0;
    }

    if (trr != 1 && trr != 2 && trr != 3) {
        trr = 0;
    }

    // These two rules are correct - a radiation pressure model with no flux has
    // nothing to act on, and fluxes with no model have nothing to drive - but
    // they used to apply silently. Setting rp_model = 3 in a config with
    // srp = 0 produced a clean run, plausible output, and no radiation pressure
    // whatsoever, with nothing to say the setting had been discarded. Say so.

    // If no craft radiation model selected then there's no need for fluxes
    if (rp_model != 1 && rp_model != 2 && rp_model != 3) {
        if (srp != 0 || erp != 0 || trr != 0) {
            std::cout << "Configuration: rp_model = " << rp_model
                      << " is not a radiation pressure model, so srp, erp and "
                         "trr have been disabled.\n";
        }
        srp = 0;
        erp = 0;
        rp_model = 0;
        trr = 0;
    }

    // Similarly, if no flux models enabled, we don't need a radiation model
    if (srp == 0 && erp == 0) {
        if (rp_model != 0 || trr != 0) {
            std::cout << "Configuration: srp = 0 and erp = 0, so rp_model = "
                      << rp_model << " and trr have been disabled - a radiation "
                         "pressure model has no flux to act on.\n";
        }
        rp_model = 0;
        trr = 0;
    }

    return true;
}

/*
 * Method to print out parameter table to stdout.
 * useful for debugging purposes.
 */
void Configuration::print() const
{
    std::cout.precision(16);

    // clang-format off
	std::cout << "--------------- Parameters used ---------------" << std::endl;

    if( t0_set ){
        std::cout << "              t0: " << t0 << " (UTC)"        << std::endl;
    }

    if( tle_set ){
    	std::cout << "            tle0: " << tle0                  << std::endl
    	          << "            tle1: " << tle1                  << std::endl
    	          << "            tle2: " << tle2                  << std::endl;
    }

    std::cout << "              x0: " << initial_state.x << " km"  << std::endl
              << "              y0: " << initial_state.y << " km"  << std::endl
              << "              z0: " << initial_state.z << " km"  << std::endl
              << "              u0: " << initial_state.u << " km/s" << std::endl
              << "              v0: " << initial_state.v << " km/s" << std::endl
              << "              w0: " << initial_state.w << " km/s" << std::endl
	          << "      spacecraft: " << spacecraft                << std::endl
              << "            mass: " << mass << " kg"             << std::endl
	          << "      propagator: " << propagator                << std::endl
	          << "       step_size: " << step_size << " s"         << std::endl
	          << " simulation_time: " << simulation_time << " s"   << std::endl
	          << " output_interval: " << output_interval << " s"   << std::endl
	          << "   output_format: " << output_format             << std::endl
	          << "   gravity_model: " << gravity_model             << std::endl
	          << "     grav_degree: " << grav_degree               << std::endl
	          << "      grav_order: " << grav_order                << std::endl
	          << "  magnetic_model: " << magnetic_model            << std::endl
	          << "      mag_degree: " << mag_degree                << std::endl
	          << "       mag_order: " << mag_order                 << std::endl
	          << " specific_charge: " << specific_charge           << std::endl
	          << "  antenna_thrust: " << antenna_thrust            << std::endl
	          << "            drag: " << drag                      << std::endl
	          << "   gr_correction: " << gr_corrections            << std::endl
	          << "             srp: " << srp                       << std::endl
	          << "             erp: " << erp                       << std::endl
	          << "             trr: " << trr                       << std::endl
	          << "        rp_model: " << rp_model                  << std::endl
	          << "      third_body: " << third_body                << std::endl
	          << "          y-bias: " << y_bias                    << std::endl
	          << "       pole_tide: " << pole_tide                 << std::endl
	          << "solid_earth_tide: " << solid_earth_tide          << std::endl
	          << "   time_var_grav: " << time_var_grav             << std::endl
	          << "        location: " << location                  << std::endl
	          << "         GMTtime: " << GMTtime                   << std::endl;
    // clang-format on
}

void Configuration::print_switches() const
{
    std::cout << "              Antenna Thrust " << antenna_thrust << std::endl
              << "                        Drag " << drag << std::endl
              << "              GR Corrections " << gr_corrections << std::endl
              << "        Magnetic Field Model " << magnetic_model
              << ((magnetic_model == 1) ? ", IGRF11" : "")
              << ((magnetic_model == 2) ? ", IGRF12" : "") << std::endl
              << "                         SRP " << srp
              << ((srp == 1) ? ", ECEF Solar flux model" : "")
              << ((srp == 2) ? ", ECI Solar flux model" : "") << std::endl
              << "                         ERP " << erp
              << ((erp == 1) ? ", ECEF Simple Earth flux model" : "")
              << ((erp == 2) ? ", ECI Simple Earth flux model" : "")
              << ((erp == 3) ? ", CERES Earth model" : "") << std::endl
              << "                         TRR " << trr
              << ((trr == 1) ? ", Stu's original 2 slab model" : "")
              << ((trr == 2) ? ", Simplified 1 slab model" : "")
              << ((trr == 3) ? ", Improved 2 slab model" : "") << std::endl
              << "    Radiation Pressure Model " << rp_model
              << ((rp_model == 1) ? ", Analytic" : "")
              << ((rp_model == 2) ? ", Gridfile" : "")
              << ((rp_model == 3) ? ", Box and Wing" : "") << std::endl
              << "          Third Body Gravity " << third_body << std::endl
              << "           Body Frame Y-Bias " << y_bias << std::endl
              << "       Pole Tide Corrections " << false << std::endl
              << "Solid Earth Tide Corrections " << solid_earth_tide
              << std::endl
              << "       Time Variable Gravity " << false << std::endl;
}

void Configuration::set_initial_state()
{
    t0_set = false;
    tle_set = false;

    long int MJDN_t0;
    std::stringstream ss;

    long int year = 1977, month = 1, date = 1, hour = 0, min = 0;
    double sec = 0.0;

    MJDN_t0 = Timetag::MJD_1_jan_1977;

    // Partially parse t0 (if it is set), enough to get the MJDN.
    if (t0.length() > 5 && t0.substr(5, 1) == ".") {

        MJDN_t0 = std::stol(t0.substr(0, 5));

        long long int sec_in_day, day_frac;
        long int SOD_t0;
        double SOD_frac_t0;

        std::string day_frac_str;

        sec_in_day = static_cast<long long int>(
            Timetag::get_seconds_in_UTC_day(MJDN_t0));

        ss << std::left << std::setfill('0') << std::setw(14) << t0.substr(6);
        day_frac_str = ss.str();

        day_frac = std::stoll(day_frac_str.substr(0, 14)) * sec_in_day;

        lldiv_t SOD_lldiv_t = std::lldiv(day_frac, 100000000000000LL);

        SOD_t0 = static_cast<long int>(SOD_lldiv_t.quot);
        SOD_frac_t0 = static_cast<double>(SOD_lldiv_t.rem) / 100000000000000.0;

        initial_state.epoch.set_timetag_from_MJD_UTC(MJDN_t0, SOD_t0,
                                                     SOD_frac_t0);

        t0_set = true;

    } else if (t0.length() > 7 && t0.substr(4, 1) == "," &&
               t0.substr(7, 1) == ",") {

        char c1, c2, c3, c4, c5;

        ss << t0;
        ss >> year >> c1 >> month >> c2 >> date >> c3 >> hour >> c4 >> min >>
            c5 >> sec;

        MJDN_t0 = Timetag::calculate_MJDN_from_cal(year, month, date);

        initial_state.epoch.set_timetag_from_cal_UTC(year, month, date, hour,
                                                     min, sec);
        t0_set = true;
    }

    // Parse TLE if set
    if (tle1.length() == 69 && tle2.length() == 69) {
        tle = Two_line_element(tle0, tle1, tle2);

        if (spacecraft == "none") {
            spacecraft = tle.get_name();
        }

        tle_set = tle.valid_tle();
    }

    if (t0_set && tle_set) {
        if (tle.set_TLE_time(initial_state.epoch)) {
            initial_state = tle.get_current_state();
        } else {
            std::cerr << "TLE entered could not be propagated to desired time."
                      << std::endl;
            std::exit(1);
        }
    } else if (!t0_set && tle_set) {
        initial_state = tle.get_epoch_state();
    } else if (t0_set && !tle_set) {
        // Do nothing, initial_state is set correctly
    } else {
        std::cerr << "Invalid format for time argument or TLE data in the "
                     "config file."
                  << std::endl;
        std::exit(1);
    }

    if (propagator == 2 && !tle_set) {
        propagator = 1;
        std::cout << "Warning: SGP4 propagator selected with no valid TLE set!"
                  << std::endl
                  << "Defaulting to ERK87 propagator instead." << std::endl
                  << std::endl;
    } else if (propagator != 2 && tle_set) {
        std::cout << "Warning: TLE is set but SGP4 propagator was not selected!"
                  << std::endl
                  << "Initial conditions have been determined from the TLE. "
                     "This works, but may not be what you intended."
                  << std::endl
                  << std::endl;
    }
}

void Configuration::set_defaults()
{
    tle0 = "";
    tle1 = "";
    tle2 = "";

    spacecraft = "none"; //!< spacecraft name or identifier
    mass = 0.0;
    area = 0.0;
    reflectivity = 0.0;
    specularity = 0.0;
    srp_scale = 0.0;
    srp_scale_amp = 0.0;
    srp_scale_period = 0.0;

    propagator = 1;
    step_size = 1.0; //!< propagator timestep (seconds)

    /**** Length of prediction run ****/
    simulation_time = 5400; //!< in seconds
    output_interval = 100;  //!< output interval
    output_format = "ops";  //!< format of output file

    gravity_model = 7; //!< 7 = GRACE-GGMO3C (360x360)
    grav_degree = 20;  //!< degree in the Earth gravity model
    grav_order = -1;   //!< order in the Earth gravity model

    magnetic_model = 0;    //!< 0 = none, 1 = IGRF11, 2 = IGRF12
    mag_degree = 13;       //!< degree in the Earth magnetic field model
    mag_order = -1;        //!< order in the Earth magnetic field model
    specific_charge = 0.0; //!< electrical charge measured in nC/kg

    antenna_thrust = false;   //!< antenna thrust
    drag = 0;             //!< drag
    gr_corrections = false;   //!< GR corrections
    srp = false;              //!< srp forces
    erp = 0;                  //!< earth radiation pressure
    trr = false;              //!< thermal re-radiation forces
    rp_model = 0;             //!< radiation pressure model
    third_body = false;       //!< default includes third body forces
                              //!< from the sun, moon, venus and jupiter
    y_bias = false;           //!< BFS Y-Bias acceleration model
    pole_tide = false;        //!< pole tide correction
    solid_earth_tide = false; //!< solid Earth tide correction
    time_var_grav = false;    //!< off by default (requires specific grav model)

    location = 0;
    GMTtime = 0;

    t0_set = false;
    tle_set = false;
}
