/*! @file Resident_constants.cpp
	@author David Harrison
	@date 12 May 2016
	@brief SGNL OPS file defining the Resident_constants class.
 */

#include "../include/Resident_constants.h"

void Resident_constants::print_properties() const
{
    std::cout << "Name: " << name << std::endl
              << "mass = " << mass << " kg" << std::endl
              << "area = " << area << " m^2" << std::endl
              << "reflectivity = " << nu << std::endl
              << "specularity = " << mu << std::endl
              << "solar array area = " << solar_array.area << std::endl;
}

void Resident_constants::setup(const Configuration &config)
{
    name = config.spacecraft;
    mass = config.mass;

    gravity_model = config.gravity_model;
    grav_degree = static_cast<size_t>(config.grav_degree);
    grav_order = static_cast<size_t>(config.grav_order);

    magnetic_model = config.magnetic_model;
    mag_degree = static_cast<size_t>(config.mag_degree);
    mag_order = static_cast<size_t>(config.mag_order);
    specific_charge = config.specific_charge;

    location = config.location;
    GMTtime = config.GMTtime;

    solar_flux_model = config.srp;
    earth_flux_model = config.erp;

    set_name_specific_values(config);

    // User configured mass overrides name-specific mass
    if (config.mass > 0.0) {
        mass = config.mass;
    }

    // Likewise for the radiation-pressure properties, so a different craft
    // can be modelled from the config file rather than by recompiling.
    if (config.area > 0.0) {
        area = config.area;
    }
    if (config.reflectivity > 0.0) {
        nu = config.reflectivity;
    }
    if (config.specularity > 0.0) {
        mu = config.specularity;
    }

    // But if the mass hasn't been set at all, make it something sensible
    if (mass <= 0.0) {
        mass = 1000.0;
    }
}

void Resident_constants::set_name_specific_values(const Configuration &config)
{
    solar_array.front.clear();
    solar_array.rear.clear();
    solar_array.yoke.clear();

    if (name == "gpsIIR") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1100.0;

        // IGS model value for GPS IIR antenna power is 85 W, source:
        // http://acc.igs.org/orbits/thrust-power.txt
        // antenna_power = 85.0; // 85 Watts in probeEarth direction
        antenna_power = 77.0; // Given in USAF report, used for JOGE paper

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 13.564096;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        // WARNING: Rear of panels needs to be updated based on real data
        solar_array.rear = solar_array.front;
        solar_array.rear[0].refl = 0.7;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;
        // solar_array.alpha[1] = 0.017;
        // Source: http://web.missouri.edu/~zhangyu/Pubs/Confs/AIAA-2005-5211.pdf
        // Table 7, absorptivity of Al2024

        // Not sure where these values came from, maybe Stu?
        // solar_array.k[0] = 1.86148;
        // solar_array.k[1] = 1.86148;

        // Updated values, 1.661278623 is the overall thermal conductivity of
        // the panel, computed by summing the thermal resistances (thickness/k)
        // for each of the layers, then dividing the overall thickness by the
        // total resistance. Panel properties from:
        // GPS Block IIR Spacecraft Structural, Optical and Thermal Data
        // Dr Marek Ziebart and Ms Sima Adhya
        // solar_array.k[0] = 1.661278623;
        // solar_array.k[1] = 1.661278623;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        // solar_array.thickness[0] = 0.0134874;
        // solar_array.thickness[1] = 0.0134874;

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 610.0;

        // Values for reflectivity and specularity of GPS IIR bus in userfile
        nu = 0.06;
        mu = 0.0;

        // Values used in GPS IIR box model in NAPEOS
        // nu = 0.055;
        // mu = 1.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(2.286568, 2.859672, 3.059184);

        // GPS IIR Box dimensions from NAPEOS
        // face_area.set(4.110, 0.0, 4.25);

        // y_bias_accel = 6.0E-14;
        y_bias_accel = -8.0E-13;

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/gpsIIR/gpsIIR_gridfiles/";

        // A grid file computed with SRP only, i.e. no MLI TRR
        // Note, only one grid file was computed for GPSIIR, didn't
        // bother computing version with and without NAP antenna     
        // x_grid_file = path + "gpsiirnap_x_srponly_q18w12.grd";
        // y_grid_file = path + "gpsiirnap_y_srponly_q22w11.grd";
        // z_grid_file = path + "gpsiirnap_z_srponly_q33w11.grd";

        x_grid_file = path + "no_nap_antenna/gpsiirX-R100-Q34W13S0.grd";
        y_grid_file = path + "no_nap_antenna/gpsiirY-R100-Q50W50S0.grd";
        z_grid_file = path + "no_nap_antenna/gpsiirZ-R100-Q32W11S0.grd";

    } else if (name == "gpsIIR_nap_antenna") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1100.0;

        // IGS model value for GPS IIR antenna power is 85 W, source:
        // http://acc.igs.org/orbits/thrust-power.txt
        // antenna_power = 85.0; // 85 Watts in probeEarth direction
        antenna_power = 77.0; // Given in USAF report, used for JOGE paper

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 13.564096;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        // WARNING: Rear of panels needs to be updated based on real data
        solar_array.rear = solar_array.front;
        solar_array.rear[0].refl = 0.7;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;
        // solar_array.alpha[1] = 0.017;
        // Source: http://web.missouri.edu/~zhangyu/Pubs/Confs/AIAA-2005-5211.pdf
        // Table 7, absorptivity of Al2024

        // Not sure where these values came from, maybe Stu?
        // solar_array.k[0] = 1.86148;
        // solar_array.k[1] = 1.86148;

        // Updated values, 1.661278623 is the overall thermal conductivity of
        // the panel, computed by summing the thermal resistances (thickness/k)
        // for each of the layers, then dividing the overall thickness by the
        // total resistance. Panel properties from:
        // GPS Block IIR Spacecraft Structural, Optical and Thermal Data
        // Dr Marek Ziebart and Ms Sima Adhya
        // solar_array.k[0] = 1.661278623;
        // solar_array.k[1] = 1.661278623;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        // solar_array.thickness[0] = 0.0134874;
        // solar_array.thickness[1] = 0.0134874;

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 610.0;

        // Values for reflectivity and specularity of GPS IIR bus in userfile
        nu = 0.06;
        mu = 0.0;

        // Values used in GPS IIR box model in NAPEOS
        // nu = 0.055;
        // mu = 1.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(2.286568, 2.859672, 3.059184);

        // GPS IIR Box dimensions from NAPEOS
        // face_area.set(4.110, 0.0, 4.25);

        // y_bias_accel = 6.0E-14;
        y_bias_accel = -8.0E-13;

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/gpsIIR/gpsIIR_gridfiles/";

        // x_grid_file = path + "gpsiirnap_x_srponly_q18w12.grd";
        // y_grid_file = path + "gpsiirnap_y_srponly_q22w11.grd";
        // z_grid_file = path + "gpsiirnap_z_srponly_q33w11.grd";

        x_grid_file = path + "nap_antenna/gpsiirnapX-R100-Q37W13S0.grd";
        y_grid_file = path + "nap_antenna/gpsiirnapY-R100-Q50W50S0.grd";
        z_grid_file = path + "nap_antenna/gpsiirnapZ-R100-Q32W11S0.grd";

    } else if (name == "testbox") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1.0;
        mass = 1.0;

        solar_array.power_frac = 1.0;

        solar_array.area = 0.0;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.0;
        solar_array.front[0].spec = 0.0;

        solar_array.rear.resize(1);

        solar_array.rear = solar_array.front;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.0;
        solar_array.yoke[0].refl = 0.0;
        solar_array.yoke[0].spec = 0.0;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 1.0;
        solar_array.alpha[1] = 1.0;

        solar_array.k[0] = 1.661278623;
        solar_array.k[1] = 1.661278623;

        solar_array.epsilon[0] = 1.0;
        solar_array.epsilon[1] = 1.0;

        solar_array.thickness[0] = 0.0134876;
        solar_array.thickness[1] = 0.0134876;

        solar_array.power = 0.0;

        nu = 0.9;
        mu = 0.9;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        face_area.set(1.0, 1.0, 1.0);
    } else if (name == "bdsIGSO") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1100.0;

        // IGS model value for GPS IIR antenna power is 85 W, source:
        // http://acc.igs.org/orbits/thrust-power.txt
        antenna_power = 85.0; // 85 Watts in probeEarth direction

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 22.704;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        // WARNING: Rear of panels needs to be updated based on real data
        solar_array.rear = solar_array.front;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.0;
        solar_array.yoke[0].refl = 0.2;
        solar_array.yoke[0].spec = 0.6;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;
        // solar_array.alpha[1] = 0.017;
        // Source: http://web.missouri.edu/~zhangyu/Pubs/Confs/AIAA-2005-5211.pdf
        // Table 7, absorptivity of Al2024

        // Not sure where these values came from, maybe Stu?
        // solar_array.k[0] = 1.86148;
        // solar_array.k[1] = 1.86148;

        // Updated values, 1.661278623 is the overall thermal conductivity of
        // the panel, computed by summing the thermal resistances (thickness/k)
        // for each of the layers, then dividing the overall thickness by the
        // total resistance. Panel properties from:
        // GPS Block IIR Spacecraft Structural, Optical and Thermal Data
        // Dr Marek Ziebart and Ms Sima Adhya
        solar_array.k[0] = 1.661278623;
        solar_array.k[1] = 1.661278623;

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        solar_array.thickness[0] = 0.0134876;
        solar_array.thickness[1] = 0.0134876;

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 1022;

        // Values for reflectivity and specularity of BDS IGSO bus
        // different face has different value !!!
        nu = 0.7;
        mu = 0.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(3.784, 4.4, 3.44);

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/BeiDou/bdsIGSO_gridfiles/";
        x_grid_file = path + "BDSIGSOX-R100-Q12W25S0.grd";
        y_grid_file = path + "BDSIGSOY-R100-Q42W11S0.grd";
        z_grid_file = path + "BDSIGSOZ-R100-Q15W43S0.grd";

    } else if (name == "bdsGEO") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1100.0;

        // IGS model value for GPS IIR antenna power is 85 W, source:
        // http://acc.igs.org/orbits/thrust-power.txt
        antenna_power = 85.0; // 85 Watts in probeEarth direction

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 13.564096;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        // WARNING: Rear of panels needs to be updated based on real data
        solar_array.rear = solar_array.front;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;
        // solar_array.alpha[1] = 0.017;
        // Source: http://web.missouri.edu/~zhangyu/Pubs/Confs/AIAA-2005-5211.pdf
        // Table 7, absorptivity of Al2024

        // Not sure where these values came from, maybe Stu?
        // solar_array.k[0] = 1.86148;
        // solar_array.k[1] = 1.86148;

        // Updated values, 1.661278623 is the overall thermal conductivity of
        // the panel, computed by summing the thermal resistances (thickness/k)
        // for each of the layers, then dividing the overall thickness by the
        // total resistance. Panel properties from:
        // GPS Block IIR Spacecraft Structural, Optical and Thermal Data
        // Dr Marek Ziebart and Ms Sima Adhya
        solar_array.k[0] = 1.661278623;
        solar_array.k[1] = 1.661278623;

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        solar_array.thickness[0] = 0.0134876;
        solar_array.thickness[1] = 0.0134876;

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 610.0;

        // Values for reflectivity and specularity of GPS IIR bus in userfile
        nu = 0.06;
        mu = 0.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(2.286568, 2.859672, 3.059184);

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/BeiDou/bdsIGSO_gridfiles/";
        x_grid_file = path + "BDSX-R100-Q21W34S0.grd";
        y_grid_file = path + "BDSY-R100-Q50W50S0.grd";
        z_grid_file = path + "BDSZ-R100-Q46W14S0.grd";

    } else if (name == "galileo_iov") {
        // nominal mass used to generate SRP grid files:
        // Galileo nominal mass value should have been 700 kg as that is the
        // mass of the IOV spacecraft, but 732.8 kg was used which is the FOC mass
        nominal_mass = 732.8; // Zhen gridfile nominal mass or 732.8
        // nominal_mass = 1.0; // Nominal mass for grid files generated by ZL code

        // Unclear which antenna thrust to use from:
        // Antenna thrust effect in Galileo satellites_iss1.pptx
        antenna_power = 276.46;
        // antenna_power = 431.68;

        // Power draw for half the solar panel area
        solar_array.power_frac = 388.0 / 1082.0;

        // Surface material properties of the solar array
        // Source: Galileo IOV Satellite Metadata
        solar_array.area = 10.82;

        solar_array.front.resize(2);

        // Material 0 is all of the solar calls
        solar_array.front[0].area = 7.76;
        solar_array.front[0].refl = 0.2435;
        solar_array.front[0].spec = 0.85;

        // Material 1 is Carbon filled Kapton
        solar_array.front[1].area = 3.06;
        solar_array.front[1].refl = 0.2435;
        solar_array.front[1].spec = 0.85;

        solar_array.rear.resize(1);

        // Assume back surface is Carbon-filled-Kapton coated like FOC satellite
        solar_array.rear[0].area = 10.82;
        solar_array.rear[0].refl = 0.08;
        solar_array.rear[0].spec = 0.0;

        solar_array.yoke.resize(1);

        // No information found for Galileo yoke arm, so using GPS IIR values
        // solar_array.yoke[0].area = 0.08;
        // solar_array.yoke[0].refl = 0.08;
        // solar_array.yoke[0].spec = 0.0;

        // Average of absorbtivities for front materials:
        solar_array.alpha[0] = 98932.0 / 108200.0;
        // Assume back surface is Carbon-filled-Kapton coated like FOC satellite
        solar_array.alpha[1] = 0.9;

        solar_array.epsilon[0] = 0.8;
        solar_array.epsilon[1] = 0.82;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        // Don't have data for Galileo power draw so copied GPS IIR
        solar_array.power = 610;

        // Values for reflectivity and specularity of BDS IGSO bus
        // different face has different value !!!
        //nu = 0.7;
        //mu = 0.0;

        // -x refl = 0.06; spec = 0.0

        // Beginning of life:
        // +x refl = (0.54*0.06 + 0.78*0.90) / 1.32; spec = 0.78*0.8 / 1.32;
        // -y refl = (1.03*0.06 + 1.97*0.90) / 3.00; spec = 1.97*0.8 / 3.00;
        // +y refl = (1.00*0.06 + 2.00*0.90) / 3.00; spec = 2.00*0.8 / 3.00;

        // End of life:
        // +x refl = (0.54*0.06 + 0.78*0.75) / 1.32; spec = 0.78*0.8 / 1.32;
        // -y refl = (1.03*0.06 + 1.97*0.75) / 3.00; spec = 1.97*0.8 / 3.00;
        // +y refl = (1.00*0.06 + 2.00*0.75) / 3.00; spec = 2.00*0.8 / 3.00;

        // -z refl = 0.06; spec = 0.0;
        // +z refl = (1.72*0.06 + 1.28*0.43) / 3.00; spec = 1.28*0.22 / (0.43*3.00);

        // For box-and-wing model
        // From GAL-TN-ESA-SYST-X2153_1.0_IOV Sat Metadata_signed.pdf
        // Section 7.2 Optical Properties
        // Using beginning of life values for +x and +-y faces
        nu_pos.set(0.7344 / 1.32, 0.62, 0.6536 / 3.0);
        mu_pos.set(0.624 / 1.32, 1.6 / 3.0, 0.2816 / 1.29);

        nu_neg.set(0.06, 0.6116, 0.18);
        mu_neg.set(0.0, 1.576 / 3.0, 0.0);

        face_area.set(1.32, 3.00, 3.00);

        y_bias_accel = -2.94E-14; // in full phase
        // y_bias_accel = -2.22E-13; // in eclipse

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/galileoIOV/galileo_iov_gridfiles/";

        // These are the lastest UCL grid file for IOV, 25 August 2017
        x_grid_file = path + "galileo_iov_x_q22w32.grd";
        y_grid_file = path + "galileo_iov_y_q50w50.grd";
        z_grid_file = path + "galileo_iov_z_q19w50.grd";

        // These are UCL grid files produced by Zhen Li's raytracer, 22Aug17
        // x_grid_file = path + "xgrid_zl_22aug2017pm.grd";
        // y_grid_file = path + "ygrid_zl_22aug2017pm.grd";
        // z_grid_file = path + "zgrid_zl_22aug2017pm.grd";

    } else if (name == "galileo_foc") {
        // nominal mass used to generate SRP grid files:
        // used the Jason-2 value, forgot to replace with the Galileo value
        // should have used about 700 kg as that is the mass of the IOV
        // spacecraft
        //nominal_mass = 696.815;
        //nominal_mass = 505.9;
        nominal_mass = 732.8; // Zhen gridfile nominal mass

        // Unclear which antenna thrust to use from:
        // Antenna thrust effect in Galileo satellites_iss1.pptx
        antenna_power = 276.46;
        // antenna_power = 431.68;

        // Power draw for half the solar panel area
        solar_array.power_frac = 388.0 / 1082.0;

        // Surface material properties of the solar array
        // Source: Galileo IOV Satellite Metadata
        solar_array.area = 10.82;

        solar_array.front.resize(2);

        // Material 0 is all of the solar calls
        solar_array.front[0].area = 7.76;
        solar_array.front[0].refl = 0.08;
        solar_array.front[0].spec = 1.0;

        // Material 1 is Carbon filled Kapton
        solar_array.front[1].area = 3.06;
        solar_array.front[1].refl = 0.1;
        solar_array.front[1].spec = 1.0;

        solar_array.rear.resize(1);

        // Assume back surface is Carbon-filled-Kapton coated like FOC satellite
        solar_array.rear[0].area = 10.82;
        solar_array.rear[0].refl = 0.1;
        solar_array.rear[0].spec = 1.0;

        solar_array.yoke.resize(1);

        // No information found for Galileo yoke arm, so using GPS IIR values
        solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Average of absorbtivities for front materials:
        solar_array.alpha[0] = 98932.0 / 108200.0;
        // Assume back surface is Carbon-filled-Kapton coated like FOC satellite
        solar_array.alpha[1] = 0.9;

        solar_array.epsilon[0] = 0.8;
        solar_array.epsilon[1] = 0.82;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        // Don't have data for Galileo power draw so copied GPS IIR
        solar_array.power = 610;

        // Values for reflectivity and specularity of BDS IGSO bus
        // different face has different value !!!
        nu = 0.7;
        mu = 0.0;

        // -x refl = 0.06; spec = 0.0

        // Beginning of life:
        // +x refl = (0.54*0.06 + 0.78*0.90) / 1.32; spec = 0.78*0.8 / 1.32;
        // -y refl = (1.03*0.06 + 1.97*0.90) / 3.00; spec = 1.97*0.8 / 3.00;
        // +y refl = (1.00*0.06 + 2.00*0.90) / 3.00; spec = 2.00*0.8 / 3.00;

        // End of life:
        // +x refl = (0.54*0.06 + 0.78*0.75) / 1.32; spec = 0.78*0.8 / 1.32;
        // -y refl = (1.03*0.06 + 1.97*0.75) / 3.00; spec = 1.97*0.8 / 3.00;
        // +y refl = (1.00*0.06 + 2.00*0.75) / 3.00; spec = 2.00*0.8 / 3.00;

        // -z refl = 0.06; spec = 0.0;
        // +z refl = (1.72*0.06 + 1.28*0.43) / 3.00; spec = 1.28*0.22 / (0.43*3.00);

        // For box-and-wing model
        // From GAL-TN-ESA-SYST-X2153_1.0_IOV Sat Metadata_signed.pdf
        // Section 7.2 Optical Properties
        // Using beginning of life values for +x and +-y faces
        nu_pos.set(0.7344 / 1.32, 0.62, 0.6536 / 3.0);
        mu_pos.set(0.624 / 1.32, 1.6 / 3.0, 0.2816 / 1.29);

        nu_neg.set(0.06, 0.6116, 0.18);
        mu_neg.set(0.0, 1.576 / 3.0, 0.0);

        face_area.set(1.32, 3.00, 3.00);

        y_bias_accel = -1.13E-12; // in full phase
        // y_bias_accel = -8.52E-13; // in eclipse

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/galileo_foc/galileo_foc_gridfiles/";

        // The latest grid files for the UCL model for FOC, 25 Aug 2017
        x_grid_file = path + "galileo_foc_x_q13w45.grd";
        y_grid_file = path + "galileo_foc_y_q50w50.grd";
        z_grid_file = path + "galileo_foc_z_q45w22.grd";

    } else if (name == "s1a") {
        // Nominal mass used to generate SRP grid files:
        nominal_mass = 2300.0;

        // **Assuming** 1500 W average, antenna can use up to 2000 W
        antenna_power = 1500.0;

        // Power draw for half the solar panel area
        solar_array.power_frac = 1.0;

        // Surface material properties of the solar array
        // Source: Sentinel-1A spaceraft key features
        // solar_array.area = 25.5694;
        solar_array.area = 28.896; // From s1_h1_bol.erg

        solar_array.front.resize(1);

        // s1_h1_bol.erg says zero specularity for panels (and MLI), this is
        // unrealistic and should be investigated further
        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.09;
        solar_array.front[0].spec = 0.0;

        solar_array.rear.resize(1);

        solar_array.rear[0].area = solar_array.area;
        solar_array.rear[0].refl = 0.08;
        solar_array.rear[0].spec = 0.0;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 3.5668;
        solar_array.yoke[0].refl = 0.08;
        solar_array.yoke[0].spec = 0.0;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.91;
        solar_array.alpha[1] = 0.92;

        solar_array.epsilon[0] = 0.81;
        solar_array.epsilon[1] = 0.82;

        // Using GPS IIR values for k and thickness for now
        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // 4900 W at end of life (~7.25 years), **assuming** 5000 W now
        solar_array.power = 5000.0;

        // These values calculated from geometry and surface properties in
        // s1_h1_bol.erg
        nu_pos.set(3.3230037120 / 4.2390360000, 0.56,
                   9.37430092812 / 12.9408600000);
        mu_pos.set(2.8824970944 / 3.3230037120, 0.70,
                   1.10551521696 / 9.37430092812);

        nu_neg.set(3.3230037120 / 4.2390360000, 4.2357079520 / 5.868865000,
                   0.56);
        mu_neg.set(2.8824970944 / 3.3230037120, 3.5213900624 / 4.235707952,
                   0.70);

        face_area.set(4.239036, 5.868865, 12.94086);

        // MLI values for cannonball model
        // Material properties in s1_h1_bol.erg claim MLI has zero specularity,
        // but this is clearly not the case from photographs of real spacecraft
        nu = 0.56;
        mu = 0.7;

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/s1a/s1a_gridfiles/";
        x_grid_file = path + "galileo_iov_x_q17w20.grd";
        y_grid_file = path + "galileo_iov_y_q42w31.grd";
        z_grid_file = path + "galileo_iov_z_q19w14.grd";

    } else if (name == "bdsGEO") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1100.0;

        // IGS model value for GPS IIR antenna power is 85 W, source:
        // http://acc.igs.org/orbits/thrust-power.txt
        antenna_power = 85.0; // 85 Watts in probeEarth direction

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 13.564096;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        // WARNING: Rear of panels needs to be updated based on real data
        solar_array.rear = solar_array.front;

        solar_array.yoke.resize(1);

        solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;
        // solar_array.alpha[1] = 0.017;
        // Source: http://web.missouri.edu/~zhangyu/Pubs/Confs/AIAA-2005-5211.pdf
        // Table 7, absorptivity of Al2024

        // Not sure where these values came from, maybe Stu?
        // solar_array.k[0] = 1.86148;
        // solar_array.k[1] = 1.86148;

        // Updated values, 1.661278623 is the overall thermal conductivity of
        // the panel, computed by summing the thermal resistances (thickness/k)
        // for each of the layers, then dividing the overall thickness by the
        // total resistance. Panel properties from:
        // GPS Block IIR Spacecraft Structural, Optical and Thermal Data
        // Dr Marek Ziebart and Ms Sima Adhya
        solar_array.k[0] = 1.661278623;
        solar_array.k[1] = 1.661278623;

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        solar_array.thickness[0] = 0.0134876;
        solar_array.thickness[1] = 0.0134876;

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 610.0;

        // Values for reflectivity and specularity of GPS IIR bus in userfile
        nu = 0.06;
        mu = 0.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(2.286568, 2.859672, 3.059184);

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/BeiDou/bdsIGSO_gridfiles/";
        x_grid_file = path + "BDSX-R100-Q21W34S0.grd";
        y_grid_file = path + "BDSY-R100-Q50W50S0.grd";
        z_grid_file = path + "BDSZ-R100-Q46W14S0.grd";
    } else if (name == "box-test") {
        // nominal mass used to generate SRP grid files:
        nominal_mass = 1000.0;

        antenna_power = 0.0;

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        // solar_array.area = 13.564096;
        solar_array.area = 0.0;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.28;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(1);

        solar_array.rear = solar_array.front;

        solar_array.yoke.resize(1);

        // solar_array.yoke[0].area = 0.30;
        solar_array.yoke[0].area = 0.0;
        solar_array.yoke[0].refl = 0.85;
        solar_array.yoke[0].spec = 0.85;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.72;
        solar_array.alpha[1] = 0.78;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.epsilon[0] = 0.86;
        solar_array.epsilon[1] = 0.89;

        // solar_array.thickness[0] = 0.0134874;
        // solar_array.thickness[1] = 0.0134874;

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        // Power drawn from only half of the 4 panels, at a power density of
        // 90 W/m^2, giving approximately 610 W total
        solar_array.power = 610.0;

        nu = 0.9;
        mu = 0.1;

        // For box-and-wing model
        nu_pos.set(0.9, 0.5, 0.9);
        mu_pos.set(0.1, 0.5, 0.1);

        nu_neg.set(0.1, 0.5, 0.1);
        mu_neg.set(0.1, 0.5, 0.1);

        face_area.set(6.624, 6.1824, 3.024);

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/box-test/gridfiles/";
        x_grid_file = path + "boxX-R100-Q13W19S0.grd";
        y_grid_file = path + "boxY-R100-Q13W19S0.grd";
        z_grid_file = path + "boxZ-R100-Q13W19S0.grd";
    } else if (name == "jason2") {
        /*!
		 These numbers are from the "Guidelines for Implementing UCL Jason-2
		 Non-conservative force models".
         February 2016, Santosh and Marek

		 Also used the "DORIS satellite models implemented in POE
		 processing" document released by the International DORIS Service in
		 2010, for solar panel area values. For the solar array values, the
		 Jason-2 numbers are used. According to this document at least, the
		 Jason-1 and Jason-2 panel properties are the same.
		 */

        nominal_mass = 505.9; // kg, this is the nominal mass.

        // Years since 4th July 2008, as used in Jason 2 mass model
        double MJD_UTC = config.initial_state.epoch.get_MJD_UTC();
        double t = (MJD_UTC - 54651.0) / 365.25;
        double delta_m;

        // clang-format off
        if (MJD_UTC < 54638.0) {
            delta_m = 0.0; // This is before mass model validity
        } else if (MJD_UTC < 54642.0) {
            delta_m = -0.631;
        } else if (MJD_UTC < 54645.0) {
            delta_m = -1.577;
        } else if (MJD_UTC < 54648.0) {
            delta_m = -2.052;
        } else if (MJD_UTC < 54651.0) {
            delta_m = -2.959;
        } else if (MJD_UTC > 57426.0) {
            delta_m = -4.560; // This is after mass model validity
        } else {
            delta_m = (((-0.000642 * t + 0.005527) * t - 0.012564) * t -
                       0.018945) * t - 3.975369;
        }
        // clang-format on

        mass = nominal_mass + delta_m;

        // Power draw for half the solar panel area
        solar_array.power_frac = 0.5;

        // Surface material properties of the solar array
        solar_array.area = 9.8;

        solar_array.front.resize(1);

        solar_array.front[0].area = solar_array.area;
        solar_array.front[0].refl = 0.25;
        solar_array.front[0].spec = 0.85;

        solar_array.rear.resize(3);

        // MLI
        solar_array.rear[0].area = solar_array.area * 0.031;
        solar_array.rear[0].refl = 0.65;
        solar_array.rear[0].spec = 0.5;

        // Black
        solar_array.rear[1].area = solar_array.area * 0.287;
        solar_array.rear[1].refl = 0.0;
        solar_array.rear[1].spec = 0.0;

        // White
        solar_array.rear[2].area = solar_array.area * 0.682;
        solar_array.rear[2].refl = 0.82;
        solar_array.rear[2].spec = 0.0;

        solar_array.yoke.resize(1);

        // WARNING: Using GPS values for the yoke for the time being
        solar_array.yoke[0].area = 0.30 * 0.725;
        solar_array.yoke[0].refl = 0.0;
        solar_array.yoke[0].spec = 0.0;

        // Thermal properties of the solar array
        solar_array.alpha[0] = 0.75;
        solar_array.alpha[1] = 0.42061; // Based off 1 - reflection coef
        // solar_array.alpha[1] = 0.78;

        solar_array.k[0] = 0.607424072; // Quartz and encapsulant
        solar_array.k[1] = 1.694884419; // Silicon cell to rear Al 2024

        solar_array.epsilon[0] = 0.82;
        solar_array.epsilon[1] = 0.82;

        // solar_array.thickness[0] = 0.0134874;
        // solar_array.thickness[1] = 0.0134874;

        solar_array.thickness[0] = 0.0003048; // Quartz and encapsulant
        solar_array.thickness[1] = 0.02667;   // Silicon cell to rear Al 2024

        solar_array.power = 90.0;

        // Values for reflectivity and specularity of GPS IIR bus in userfile
        nu = 0.06;
        mu = 0.0;

        // For box-and-wing model
        nu_pos.set(nu, nu, nu);
        mu_pos.set(mu, mu, mu);
        nu_neg = nu_pos;
        mu_neg = mu_pos;

        // Areas calculated from measurements used in GPS IIR userfile
        face_area.set(2.286568, 2.859672, 3.059184);

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/jason2/jason2_gridfiles/";
        x_grid_file = path + "jason2X-R100-Q43W16S0.grd";
        y_grid_file = path + "jason2Y-R100-Q50W50S0.grd";
        z_grid_file = path + "jason2Z-R100-Q43W11S0.grd";

        /***
    	 for crude drag testing
    	***/
        area = 8.0; //~m^2
        drag_coeff = 2.2;

    } else if (name == "spot4") {
        // To do: Populate Spot 4 data, mass, nominal mass, solar_array

        nominal_flux = sgnlOPS::nominal_solar_flux;
        path = "../analyses/spot4/spot4_gridfiles/";
        x_grid_file = path + "SPOT4X-R100-Q32W11S0.grd";
        y_grid_file = path + "SPOT4Y-R100-Q50W50S0.grd";
        z_grid_file = path + "SPOT4Z-R100-Q48W12S0.grd";
    } else if (name == "1kg_5m_black_sphere") {
        mass = 1.0;
        area = (5 * 5 / 4) * sgnlOPS::D_PI;
        drag_coeff = 2.2;
        nu = 0.0;
        mu = 0.0;
    } else if (name == "1kg_1m_black_sphere") {
        mass = 1.0;
        area = sgnlOPS::D_PI / 4;
        drag_coeff = 2.2;
        nu = 0.0;
        mu = 0.0;
    } else if (name == "1kg_5m_specular_sphere") {
        mass = 1.0;
        area = (5 * 5 / 4) * sgnlOPS::D_PI;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 1.0;
    } else if (name == "1kg_1m_specular_sphere") {
        mass = 1.0;
        area = sgnlOPS::D_PI / 4;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 1.0;
    } else if (name == "1kg_5m_diffuse_sphere") {
        mass = 1.0;
        area = (5 * 5 / 4) * sgnlOPS::D_PI;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    } else if (name == "1kg_1m_diffuse_sphere") {
        mass = 1.0;
        area = sgnlOPS::D_PI / 4;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    } else if (name == "1000kg_1m_diffuse_sphere") {
        mass = 1000.0;
        area = sgnlOPS::D_PI / 4;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Small Sphere HAMR 0.05kg 0.05m diameter -2.51e-07 ms-2
    else if (name == "Small_Sphere_HAMR") {
        mass = 0.05;
        area = sgnlOPS::D_PI * 0.05 * 0.05;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Sphere with Cubesat Properties 1.33kg 0.1m diameter
    else if (name == "Sphere_w_Cubesat_Properties") {
        mass = 1.33;
        area = sgnlOPS::D_PI * 0.1 * 0.1;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Small Sphere LAMR 0.5kg 0.05m diameter -2.51e-08 ms-2
    else if (name == "Small_Sphere_LAMR") {
        mass = 0.5;
        area = sgnlOPS::D_PI * 0.05 * 0.05;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Medium Sphere HAMR 0.5kg 1.0m diameter -4.01e-05 ms-2
    else if (name == "Medium_Sphere_LAMR") {
        mass = 0.5;
        area = sgnlOPS::D_PI * 1.0 * 1.0;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Large Sphere HAMR 5.0kg 5.0m diameter -2.50e-05 ms-2
    else if (name == "Large_Sphere_HAMR") {
        mass = 5.0;
        area = sgnlOPS::D_PI * 2.5 * 2.5;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Large Sphere LAMR 1000.0kg 5.0m diameter -2.50e-05 ms-2
    else if (name == "Large_Sphere_HAMR") {
        mass = 1000.0;
        area = sgnlOPS::D_PI * 2.5 * 2.5;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Cone HAMR 2.0kg 2.0m length 1.0m diameter -2.05e-06 ms-2
    else if (name == "Cone_HAMR") {
        mass = 2.0;
        area = 2.0 * 1.0 * 0.5;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Cone LAMR 1000.0kg 2.0m length 1.0m diameter -2.05e-06 ms-2
    else if (name == "Cone_LAMR") {
        mass = 1000.0;
        area = 2.0 * 1.0 * 0.5;
        drag_coeff = 2.2;
        nu = 1.0;
        mu = 0.0;
    }
    // Sputnik 83.60kg 0.585m diameter
    else if (name == "Sputnik") {
        mass = 83.60;
        area = (0.585 * 0.585 / 4) * sgnlOPS::D_PI;
        drag_coeff = 2.4;
        nu = 1.0;
        mu = 0.0;
    }
    // N-Prize 20g 4cm x 5cm cross section
    else if (name == "nprize") {
        mass = 0.020;
        area = 0.002;
        drag_coeff = 2.4;
        nu = 1.0;
        mu = 0.0;
    }
}
