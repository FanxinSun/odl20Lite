#include <odl/frames/transform.hpp>

extern "C" {
#include <erfa.h>
#include <erfaextra.h>
}

#include <cmath>
#include <string>

namespace odl::frames {
namespace {

using odl::time::Epoch;
using odl::time::LeapTable;
using odl::time::TimeScale;

/// dERA/dUT1 = 2*pi * 1.00273781191135448 / 86400, differentiated from the
/// defining expression of TN36 eq. (5.14).  TN36 Table 1.1 quotes a nominal mean
/// 7.292115e-5 rad/s; this is the value consistent with the rotation actually
/// applied, and the two differ by 2e-8 relative.
constexpr double kEraRate = 7.292115146706979e-5;
constexpr double kArcsecToRad = 3.14159265358979323846 / (180.0 * 3600.0);

Mat3 from_erfa(double m[3][3]) noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = m[i][j];
    return out;
}

/// Everything both chains need from the epoch and the EOP.
struct Common {
    double tt1, tt2;
    double ut1_1, ut1_2;
    Mat3   polar;        // TIRS -> ITRS
    double omega;        // rad/s, corrected for LOD
};

odl::Result<Common, FrameError> common(const Epoch& when, const odl::eop::EopRecord& eop,
                                        const LeapTable& leaps) {
    // FRAME-F-003: the sub-daily ocean-tide and libration terms must have been
    // restored.  Using the raw interpolated IERS values omits up to about
    // 0.5 mas in the pole — roughly 17 mm at 7000 km.
    if (!eop.subdaily_applied) {
        return odl::err(FrameError{"FRAME-F-003",
            "the EOP record for this epoch has subdaily_applied = false. The published IERS "
            "series are regularised: the diurnal and semi-diurnal ocean-tide and libration "
            "signals have been removed and must be added back (TN36 eq. 5.11). Omitting them "
            "costs up to about 0.5 mas in the pole and 25 us in UT1, which is 17 mm and 13 mm "
            "of satellite position at 7000 km. Use EopSeries::at(), not raw_at()."});
    }
    auto tt = when.two_part_jd(TimeScale::TT, leaps);
    if (!tt.has_value()) return odl::err(tt.error());
    auto ut1 = when.ut1_two_part_jd(odl::time::Duration::from_seconds(eop.dut1), leaps);
    if (!ut1.has_value()) return odl::err(ut1.error());

    Common c;
    c.tt1 = tt->day; c.tt2 = tt->fraction;
    c.ut1_1 = ut1->day; c.ut1_2 = ut1->fraction;

    // W(t): polar motion, including the TIO locator s'.  FRAME-R-015: s' reaches
    // about 13 uas by 2026, which is 0.43 mm at 7000 km — below a millimetre but
    // above zero, and it costs one multiplication.
    const double sp = eraSp00(c.tt1, c.tt2);
    double rpom[3][3];
    eraPom00(eop.xp, eop.yp, sp, rpom);
    c.polar = from_erfa(rpom);

    // FRAME-S-021: the LOD correction changes omega by 1-3e-8 relative, i.e.
    // about 6-18 um/s at 7000 km.  Applied, and the neglected precession-nutation
    // rate term is of the same size, which the module's documentation says.
    c.omega = kEraRate * (1.0 - eop.lod / 86400.0);
    return c;
}

/// The transport theorem, with omega applied in the frame that actually spins.
///
///   v_target = post . ( pre v_source  -  omega x (pre r_source) )
///
/// The inverse undoes exactly that, so the round trip is exact rather than
/// merely close (FRAME-R-020).
Vec3 rotate_velocity(const Rotation& rot, const Vec3& r_src, const Vec3& v_src) noexcept {
    const Vec3 r_mid = rot.pre.apply(r_src);
    const Vec3 v_mid = rot.pre.apply(v_src) - rot.omega_rad_s.cross(r_mid);
    return rot.post.apply(v_mid);
}

Vec3 unrotate_velocity(const Rotation& rot, const Vec3& r_dst, const Vec3& v_dst) noexcept {
    const Vec3 r_mid = rot.post.transpose().apply(r_dst);
    const Vec3 v_mid = rot.post.transpose().apply(v_dst) + rot.omega_rad_s.cross(r_mid);
    return rot.pre.transpose().apply(v_mid);
}

}  // namespace

ModelVersion model_version() noexcept {
    ModelVersion v;
    v.erfa = eraVersion();
    return v;
}

odl::Result<Rotation, FrameError> gcrs_to_itrs(const Epoch& when,
                                                const odl::eop::EopRecord& eop,
                                                const LeapTable& leaps) {
    auto c = common(when, eop, leaps);
    if (!c.has_value()) return odl::err(c.error());

    // Q(t): the CIP coordinates and the CIO locator.
    double x = 0.0, y = 0.0;
    eraXy06(c->tt1, c->tt2, &x, &y);
    // FRAME-R-011: s is evaluated from the MODEL X, Y, before the observed
    // offsets are applied.  TN36 §5.5.3 says the quantity s "must be considered
    // as independent of observations". The difference is below 1 uas, so this is
    // a convention requirement rather than an accuracy one — but two
    // implementations disagreeing on it will differ at a level someone will
    // later spend a day chasing.
    const double s = eraS06(c->tt1, c->tt2, x, y);
    x += eop.dx;                       // FRAME-R-012
    y += eop.dy;

    double rc2i[3][3];
    eraC2ixys(x, y, s, rc2i);          // GCRS -> CIRS

    const double era = eraEra00(c->ut1_1, c->ut1_2);

    double rpom[3][3];
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) rpom[i][j] = c->polar.r[i][j];

    double rc2t[3][3];
    eraC2tcio(rc2i, era, rpom, rc2t);  // GCRS -> ITRS

    // GCRS -> TIRS, the part before polar motion: R3(ERA) . Q.
    double rc2ti[3][3];
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) rc2ti[i][j] = rc2i[i][j];
    eraRz(era, rc2ti);

    Rotation out;
    out.m = from_erfa(rc2t);
    out.pre = from_erfa(rc2ti);
    out.post = c->polar;
    out.omega_rad_s = Vec3{0.0, 0.0, c->omega};
    return out;
}

odl::Result<Rotation, FrameError> teme_to_itrs(const Epoch& when,
                                                const odl::eop::EopRecord& eop,
                                                const LeapTable& leaps) {
    auto c = common(when, eop, leaps);
    if (!c.has_value()) return odl::err(c.error());

    // Vallado eq. (1): r_PEF = R3(theta_GMST82) r_TEME, with GMST at UT1.
    // PEF is identified with TIRS, which is unavoidable if TEME is to reach a
    // modern frame at all, and imports the difference between the IAU-76/80
    // realisation of the true pole and the IAU 2006/2000A CIP — of order
    // 0.1 arcsec, about 3 m at 7000 km. That floor is a property of TEME, not of
    // this implementation, and State<TEME> carries it (FRAME-R-033).
    // Vallado Appendix C eq. (C-1) is
    //     r_ITRF = [ROT3(theta_GMST1982) ROT3(EqEquinox1982*kin)] r_TEME
    // so the rotation carries the KINEMATIC part of the equation of the
    // equinoxes as well as GMST.  That is the two terms introduced in 1997,
    //     0.00264" sin(Omega) + 0.000063" sin(2 Omega),
    // and it is NOT the ambiguous part of the paper's discussion: the
    // ambiguities Vallado enumerates are in the GEOMETRIC nutation terms, which
    // this chain does not use at all.  Omitting it cost 85 mm on the paper's own
    // worked example, which is how it was found.
    const double om = eraFaom03(((c->tt1 - 2451545.0) + c->tt2) / 36525.0);
    const double eq_kin = (0.00264 * std::sin(om) + 0.000063 * std::sin(2.0 * om))
                          * kArcsecToRad;
    const double gmst = eraGmst82(c->ut1_1, c->ut1_2) + eq_kin;
    const double cg = std::cos(gmst), sg = std::sin(gmst);
    Mat3 r3;
    r3.r[0][0] =  cg; r3.r[0][1] = sg;  r3.r[0][2] = 0.0;
    r3.r[1][0] = -sg; r3.r[1][1] = cg;  r3.r[1][2] = 0.0;
    r3.r[2][0] = 0.0; r3.r[2][1] = 0.0; r3.r[2][2] = 1.0;

    Rotation out;
    out.pre = r3;                      // TEME -> PEF (= TIRS)
    out.post = c->polar;               // PEF -> ITRS
    out.m = out.post.times(out.pre);
    out.omega_rad_s = Vec3{0.0, 0.0, c->omega};
    return out;
}

// --------------------------------------------------------------------------- //

namespace {

template <Frame To, Frame From>
odl::Result<State<To>, FrameError> apply_forward(const State<From>& s, const Rotation& rot) {
    return State<To>{s.epoch(), rot.m.apply(s.position()),
                     rotate_velocity(rot, s.position(), s.velocity())};
}

template <Frame To, Frame From>
odl::Result<State<To>, FrameError> apply_inverse(const State<From>& s, const Rotation& rot) {
    return State<To>{s.epoch(), rot.m.transpose().apply(s.position()),
                     unrotate_velocity(rot, s.position(), s.velocity())};
}

}  // namespace

odl::Result<ItrsState, FrameError> to_itrs(const GcrsState& s, const odl::eop::EopRecord& eop,
                                            const LeapTable& leaps) {
    auto rot = gcrs_to_itrs(s.epoch(), eop, leaps);
    if (!rot.has_value()) return odl::err(rot.error());
    return apply_forward<Frame::ITRS>(s, *rot);
}

odl::Result<GcrsState, FrameError> to_gcrs(const ItrsState& s, const odl::eop::EopRecord& eop,
                                            const LeapTable& leaps) {
    auto rot = gcrs_to_itrs(s.epoch(), eop, leaps);
    if (!rot.has_value()) return odl::err(rot.error());
    return apply_inverse<Frame::GCRS>(s, *rot);
}

odl::Result<ItrsState, FrameError> to_itrs(const TemeState& s, const odl::eop::EopRecord& eop,
                                            const LeapTable& leaps) {
    auto rot = teme_to_itrs(s.epoch(), eop, leaps);
    if (!rot.has_value()) return odl::err(rot.error());
    return apply_forward<Frame::ITRS>(s, *rot);
}

odl::Result<TemeState, FrameError> to_teme(const ItrsState& s, const odl::eop::EopRecord& eop,
                                            const LeapTable& leaps) {
    auto rot = teme_to_itrs(s.epoch(), eop, leaps);
    if (!rot.has_value()) return odl::err(rot.error());
    return apply_inverse<Frame::TEME>(s, *rot);
}

odl::Result<GcrsState, FrameError> to_gcrs(const TemeState& s, const odl::eop::EopRecord& eop,
                                            const LeapTable& leaps) {
    auto itrs = to_itrs(s, eop, leaps);
    if (!itrs.has_value()) return odl::err(itrs.error());
    return to_gcrs(*itrs, eop, leaps);
}

}  // namespace odl::frames
