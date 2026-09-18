#include <odl/gravity/secular_pole.hpp>

#include <cstdint>

#include <cmath>
#include <sstream>

namespace odl::gravity {

namespace {
// 1 milliarcsecond in radians.  TN36-7 (21) gives the secular pole in mas and
// TN36-6 (6.5) wants radians; the conversion appears once, here, because this
// module's whole reason for exporting the pole is that it be defined once
// (GRAV-R-029).
constexpr double kMasToRad = 3.14159265358979323846 / (180.0 * 3600.0 * 1000.0);
constexpr double kJ2000Jd  = 2451545.0;
}  // namespace

double SecularPole::years_from_2000(const odl::time::Epoch& tt) noexcept {
    // GRAV-R-024: TT, in years of 365.25 days from J2000.0.
    //
    // Deliberately NOT through Epoch::two_part_jd, which requires a LeapTable.
    // Nothing here is a function of leap seconds: TT = TAI + 32.184 s exactly,
    // and demanding a leap-second table for a quantity that does not depend on
    // one would make this module fail for a reason unrelated to what it computes
    // — and would tempt a caller to pass an empty table.
    //
    //   J2000.0 = JD 2451545.0 TT = MJD 51544.5 TT
    //   the Epoch origin is MJD 36204 = 1958-01-01T00:00:00 TAI
    //   so J2000.0 is (51544.5 - 36204) x 86400 = 1 325 419 200 s TT after it.
    constexpr double kTtMinusTai = 32.184;
    constexpr std::int64_t kJ2000TtSeconds = 1325419200;
    const double seconds_tt = static_cast<double>(tt.tai_seconds() - kJ2000TtSeconds)
                            + tt.tai_fraction() + kTtMinusTai;
    return seconds_tt / (86400.0 * 365.25);
}

PoleCoordinates SecularPole::at_years(double t) noexcept {
    return PoleCoordinates{(kX0Mas + kXRateMasPerYear * t) * kMasToRad,
                           (kY0Mas + kYRateMasPerYear * t) * kMasToRad};
}

odl::Result<PoleCoordinates, GravityError>
SecularPole::at(const odl::time::Epoch& tt, bool extrapolate_secular_terms_beyond_fit) {
    const double t = years_from_2000(tt);
    const double year = 2000.0 + t;
    if ((year < kFitFirstYear || year > kFitLastYear) && !extrapolate_secular_terms_beyond_fit) {
        // GRAV-F-006, with the single named override of R-ERR-3.  The
        // Conventions say in §6.1 that the low-degree trends "are not strictly
        // linear in reality" and "may not be consistent with more recent surface
        // mass trends due to increased ice sheet melting", so refusing is the
        // honest default; but a refusal with no way past it would make the
        // module useless for any epoch after 2017, which is every epoch anyone
        // will use it for.
        std::ostringstream m;
        m.precision(9);
        m << "the conventional model's epoch dependence is requested outside the only published "
             "validity span this tree has.\n"
             "  requested   " << year << " (TT, years of 365.25 days)\n"
             "  fitted span " << kFitFirstYear << " to " << kFitLastYear
          << ", the least-squares fit of TN36-7 §7.1.4, from polar motion observations\n"
             "  affected    the secular pole of TN36-7 (21), which feeds the figure-axis terms\n"
             "              C21 and S21 of TN36-6 (6.5); and, by the same argument, Table 6.2's\n"
             "              secular rates, for which no span is published at all\n"
             "  override    extrapolate_secular_terms_beyond_fit, set per run, recorded in the\n"
             "              run's provenance together with the epoch it was used for. It is the\n"
             "              only override this specification has.";
        return odl::err(GravityError{"GRAV-F-006", m.str()});
    }
    return at_years(t);
}

}  // namespace odl::gravity
