#include <odl/ephemerides/ephemeris.hpp>

extern "C" {
#include <calceph.h>
}

#include <cmath>
#include <filesystem>

namespace odl::eph {
namespace {

using odl::time::Epoch;
using odl::time::LeapTable;
using odl::time::TimeScale;

/// EPH-R-010.  The unit flags are written HERE, once, and every call names this
/// constant.  CALCEPH's default is AU and AU/day and is documented and correct;
/// the objection is that it is INVISIBLE AT THE CALL SITE. A reader of
/// calceph_compute(eph, jd0, t, target, centre, pv) cannot see what pv is in.
///
/// EPH-R-011: asking the library for km and seconds means the factor of
/// 1.495978707e8 never appears in this tree's source at all. A conversion
/// written correctly today is still a conversion someone can edit.
constexpr int kUnits = CALCEPH_UNIT_KM | CALCEPH_UNIT_SEC;

/// The kernel's own body 16 is TT−TDB. This tree's convention is TDB−TT
/// (EPH-R-004), so the sign is flipped exactly once, here.
constexpr int kTimeScaleDifferenceTarget = 16;

}  // namespace

struct Ephemeris::Impl {
    t_calcephbin* handle = nullptr;
    ~Impl() { if (handle) calceph_close(handle); }
};

Ephemeris::Ephemeris(Ephemeris&&) noexcept = default;
Ephemeris& Ephemeris::operator=(Ephemeris&&) noexcept = default;
Ephemeris::~Ephemeris() = default;

odl::Result<Ephemeris, EphError> Ephemeris::open(const std::vector<std::string>& cache_paths,
                                                  std::vector<EphProvenance> provenance) {
    if (cache_paths.empty()) {
        return odl::err(EphError{"EPH-F-003", "no kernel paths were given"});
    }
    for (const std::string& p : cache_paths) {
        if (!std::filesystem::exists(p)) {
            return odl::err(EphError{"EPH-F-003",
                "kernel path does not exist: " + p +
                ". Kernels are manifest inputs (EPH-R-040); the fetcher populates the cache and "
                "verifies the hash before any path is handed here."});
        }
    }
    std::vector<const char*> c_paths;
    c_paths.reserve(cache_paths.size());
    for (const std::string& p : cache_paths) c_paths.push_back(p.c_str());

    Ephemeris e;
    e.impl_ = std::make_unique<Impl>();
    e.impl_->handle = calceph_open_array(static_cast<int>(c_paths.size()), c_paths.data());
    if (e.impl_->handle == nullptr) {
        std::string joined;
        for (const std::string& p : cache_paths) joined += (joined.empty() ? "" : ", ") + p;
        return odl::err(EphError{"EPH-F-006",
            "calceph_open_array refused the kernels: " + joined});
    }
    e.provenance_ = std::move(provenance);

    // EPH-R-012 / EPH-F-004, as amended at spec v1.2.  An SPK kernel carries NO
    // constants — measured, a .bsp reports a constant count of zero — so "read
    // the AU from the kernel" is unsatisfiable on the route the plan mandates.
    // Since IAU 2012 Resolution B2 the au is a DEFINING constant, so the
    // definition is the authority and a kernel that carries one is CHECKED
    // against it rather than believed.
    double au = 0.0;
    if (calceph_getconstant(e.impl_->handle, "AU", &au) != 0 && au != 0.0 &&
        std::abs(au - kAstronomicalUnitKm) > 1e-6) {
        return odl::err(EphError{"EPH-F-004",
            "the kernel's AU constant is " + std::to_string(au) + " km; this tree requires " +
            std::to_string(kAstronomicalUnitKm) + " km, the IAU 2012 defining value that DE440 "
            "adopts [PARK21 §2]. A kernel on a different AU cannot be compared against "
            "testpo.440, whose values are in AU."});
    }
    return e;
}

Ephemeris::AstronomicalUnit Ephemeris::astronomical_unit() const noexcept {
    double au = 0.0;
    if (calceph_getconstant(impl_->handle, "AU", &au) != 0 && au != 0.0) {
        return AstronomicalUnit{au, true};
    }
    return AstronomicalUnit{kAstronomicalUnitKm, false};
}

odl::Result<double, EphError> Ephemeris::constant(const std::string& name) const {
    double v = 0.0;
    if (calceph_getconstant(impl_->handle, name.c_str(), &v) == 0) {
        return odl::err(EphError{"EPH-F-006",
            "the kernel has no constant named '" + name + "'"});
    }
    return v;
}

odl::Result<Coverage, EphError> Ephemeris::coverage(Body b) const {
    // EPH-R-041: per BODY per kernel.  A kernel covers different bodies over
    // different spans, so scanning the position records for this body is the
    // only honest answer; "the kernel's interval" would be a different question.
    const int n = calceph_getpositionrecordcount(impl_->handle);
    Coverage cov{0.0, 0.0};
    bool found = false;

    // The solar-system barycentre is the ROOT of an SPK's body tree: it appears
    // as the centre of records and never as the target of one, so scanning for
    // it finds nothing. Measured, not assumed — de440.bsp has fourteen records
    // and every one of them has centre 0. Its coverage is therefore the span
    // over which anything is available, and without this every testpo case whose
    // target is the SSB — 868 of 13 201 — was silently counted as "outside
    // coverage" and skipped.
    const int want = classic_id(b);
    const bool is_root = (b == Body::SolarSystemBarycentre);
    for (int i = 1; i <= n; ++i) {
        int target = 0, centre = 0, frame = 0;
        double first = 0.0, last = 0.0;
        if (calceph_getpositionrecordindex(impl_->handle, i, &target, &centre, &first, &last,
                                           &frame) == 0) {
            continue;
        }
        // Records are indexed by NAIF id; map back through the same table the
        // rest of the module uses rather than a second one.
        const int classic = (target == 199 || target == 1)   ? 1
                          : (target == 299 || target == 2)   ? 2
                          : (target == 399)                  ? 3
                          : (target == 4)                    ? 4
                          : (target == 5)                    ? 5
                          : (target == 6)                    ? 6
                          : (target == 7)                    ? 7
                          : (target == 8)                    ? 8
                          : (target == 9)                    ? 9
                          : (target == 301)                  ? 10
                          : (target == 10)                   ? 11
                          : (target == 0)                    ? 12
                          : (target == 3)                    ? 13
                          : -1;
        if (!is_root && classic != want) continue;
        if (!found) { cov.first_jd_tdb = first; cov.last_jd_tdb = last; found = true; }
        else {
            cov.first_jd_tdb = std::min(cov.first_jd_tdb, first);
            cov.last_jd_tdb = std::max(cov.last_jd_tdb, last);
        }
    }
    if (!found) {
        return odl::err(EphError{"EPH-F-003",
            std::string("no loaded kernel carries ") + std::string(name_of(b)) +
            "; " + std::to_string(n) + " position records were searched"});
    }
    return cov;
}

odl::Result<Ephemeris::RelativeState, EphError>
Ephemeris::relative_state(Body target, Body centre, const Epoch& when,
                          const LeapTable& leaps) const {
    // EPH-R-001/002: TDB, as a two-part Julian date, passed through as two
    // arguments. CALCEPH's own documentation says the split is what gives the
    // interpolation its precision; collapsing it reintroduces the 40 µs
    // quantisation SPEC-time §4.2 disqualifies, which is 21 km of the Earth's
    // barycentric motion.
    auto jd = when.two_part_jd(TimeScale::TDB, leaps);
    if (!jd.has_value()) return odl::err(jd.error());

    auto cov = coverage(target);
    if (!cov.has_value()) return odl::err(cov.error());
    const double t = jd->day + jd->fraction;
    if (t < cov->first_jd_tdb || t > cov->last_jd_tdb) {
        return odl::err(EphError{"EPH-F-002",
            "requested JD(TDB) " + std::to_string(t) + " for " + std::string(name_of(target)) +
            ", whose coverage in the loaded kernels is JD " + std::to_string(cov->first_jd_tdb) +
            " to " + std::to_string(cov->last_jd_tdb) +
            ". There is no extrapolation: a Chebyshev polynomial outside its interval does not "
            "degrade, it diverges, and it does so smoothly enough to look like an orbit."});
    }

    double pv[6] = {0, 0, 0, 0, 0, 0};
    if (calceph_compute_unit(impl_->handle, jd->day, jd->fraction, classic_id(target),
                             classic_id(centre), kUnits, pv) == 0) {
        return odl::err(EphError{"EPH-F-006",
            "calceph_compute_unit refused " + std::string(name_of(target)) + " relative to " +
            std::string(name_of(centre)) + " at JD(TDB) " + std::to_string(t)});
    }
    return RelativeState{target, centre, when, odl::Vec3{pv[0], pv[1], pv[2]},
                         odl::Vec3{pv[3], pv[4], pv[5]}};
}

odl::Result<odl::frames::State<odl::frames::Frame::BCRS>, EphError>
Ephemeris::barycentric_state(Body target, const Epoch& when, const LeapTable& leaps) const {
    auto r = relative_state(target, Body::SolarSystemBarycentre, when, leaps);
    if (!r.has_value()) return odl::err(r.error());
    return odl::frames::State<odl::frames::Frame::BCRS>{when, r->position_km, r->velocity_km_s};
}

odl::Result<odl::frames::State<odl::frames::Frame::GCRS>, EphError>
Ephemeris::geocentric_state(Body target, const Epoch& when, const LeapTable& leaps) const {
    if (target == Body::Earth) {
        return odl::err(EphError{"EPH-F-008",
            "the Earth's geocentric state is identically zero and asking for it is almost always "
            "a mistake about which body was wanted. Ask for a body other than the Earth, or use "
            "relative_state if a zero vector really is intended."});
    }
    auto r = relative_state(target, Body::Earth, when, leaps);
    if (!r.has_value()) return odl::err(r.error());
    return odl::frames::State<odl::frames::Frame::GCRS>{when, r->position_km, r->velocity_km_s};
}

odl::Result<odl::time::Duration, EphError>
Ephemeris::tdb_minus_tt(const Epoch& when, const LeapTable& leaps) const {
    auto jd = when.two_part_jd(TimeScale::TDB, leaps);
    if (!jd.has_value()) return odl::err(jd.error());
    double pv[6] = {0, 0, 0, 0, 0, 0};
    if (calceph_compute_unit(impl_->handle, jd->day, jd->fraction, kTimeScaleDifferenceTarget,
                             classic_id(Body::Earth), CALCEPH_UNIT_SEC, pv) == 0) {
        return odl::err(EphError{"EPH-F-006",
            "this kernel carries no TT-TDB record (CALCEPH target 16)"});
    }
    // The kernel gives TT − TDB; this tree's convention is TDB − TT (EPH-R-004).
    // The sign is flipped once, here, and nowhere else.
    return odl::time::Duration::from_seconds(-pv[0]);
}

}  // namespace odl::eph
