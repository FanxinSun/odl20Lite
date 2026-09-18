#include <odl/gravity/scaling.hpp>

#include <cmath>
#include <sstream>

namespace odl::gravity {

namespace {
constexpr double kEgm2008GmTt  = 3.986004415e14;
constexpr double kEgm2008GmTcg = 3.986004418e14;
constexpr double kEgm2008GmTdb = 3.986004356e14;
constexpr double kEgm2008Ae    = 6378136.3;

std::string metres(double v) {
    std::ostringstream os;
    os.precision(12);
    os << v;
    return os.str();
}
}  // namespace

odl::Result<ScalingParameters, GravityError>
ScalingParameters::checked(double gm_m3_s2, double ae_m, GmCompatibility compat, std::string source) {
    const bool gm_known = gm_m3_s2 == kEgm2008GmTt || gm_m3_s2 == kEgm2008GmTcg
                       || gm_m3_s2 == kEgm2008GmTdb;
    if (!gm_known || ae_m != kEgm2008Ae) {
        // GRAV-F-005.  The diagnostic names BOTH values, BOTH sources and which
        // time scale each GM is compatible with, because the failure this
        // catches is not a typo: it is WGS 84's GM, which is a real published
        // constant that is simply not this model's.
        std::ostringstream m;
        m << "scaling parameters are not EGM2008's own, and GM and a_e must travel together "
             "(GRAV-R-004).\n"
             "  offered   GM = " << metres(gm_m3_s2) << " m^3 s^-2, a_e = " << metres(ae_m)
          << " m, declared " << name_of(compat) << "-compatible, from: " << source << "\n"
             "  the model GM = " << metres(kEgm2008GmTt) << " (TT), " << metres(kEgm2008GmTcg)
          << " (TCG), " << metres(kEgm2008GmTdb) << " (TDB); a_e = " << metres(kEgm2008Ae)
          << " m, from TN36-6 §6.1 and the EGM2008 README (2)\n"
             "  Note that WGS 84 Table 3.1's GM = 3986004.418e8 m^3 s^-2 is the TCG-compatible\n"
             "  value under another name. Used with EGM2008's coefficients on a TT argument it\n"
             "  costs 108.91 mm at 7331 km over one revolution, and secularly, because a constant\n"
             "  fractional error in GM does accumulate (SPEC-gravity GRAV-P-4).";
        return odl::err(GravityError{"GRAV-F-005", m.str()});
    }
    return ScalingParameters{gm_m3_s2, ae_m, compat, std::move(source)};
}

}  // namespace odl::gravity
