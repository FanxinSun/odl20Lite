#pragma once
// odl/thirdbody/attraction.hpp — the direct and indirect attraction of the Sun,
// the Moon and the planets on an Earth satellite.
//
// THE INDIRECT TERM IS NOT OPTIONAL (PERT-R-050).  The acceleration of the
// satellite relative to the EARTH is the body's pull on the satellite minus its
// pull on the Earth:
//
//     a = GM_j [ (r_j - r)/|r_j - r|^3  -  r_j/|r_j|^3 ]
//
// and the second term is of the same order as the first.  Dropping it is the
// classic error in this model and it does not look like one: the result stays
// the right order of magnitude and points roughly the right way.
//
// THE GM VALUES DO NOT COME FROM THE KERNEL THAT SUPPLIES THE POSITIONS.  A
// DE440 SPK carries no constants at all (SPEC-ephemerides EPH-R-012), so they
// come from NAIF's gm_de440.tpc, pinned separately and parsed here.

#include <odl/core/result.hpp>
#include <odl/ephemerides/body.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/vector.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <string>
#include <vector>

namespace odl::thirdbody {

using ThirdBodyError = odl::Diagnostic;

/// The GM of each body, in m^3 s^-2, from a pinned NAIF text PCK.
class GravitationalParameters {
public:
    /// PERT-F-010: the file must be inside the manifest cache.
    [[nodiscard]] static odl::Result<GravitationalParameters, ThirdBodyError>
    load(const std::string& path, const std::string& cache_root);

    /// PERT-F-012: a body with no pinned GM is refused, naming the body and the
    /// file searched.  Guessing one is how a 1e-11 m/s^2 term becomes a wrong
    /// 1e-11 m/s^2 term.
    [[nodiscard]] odl::Result<double, ThirdBodyError> gm_m3_s2(eph::Body b) const;

    [[nodiscard]] std::size_t count() const noexcept { return by_naif_.size(); }
    [[nodiscard]] const std::string& source_path() const noexcept { return path_; }

private:
    // Only load() constructs one: a set of gravitational parameters assembled
    // from numbers a caller happens to have is not a pinned set.
    GravitationalParameters() = default;
    std::vector<std::pair<int, double>> by_naif_;   ///< NAIF id -> GM, km^3/s^2 as read
    std::string path_;
};

struct NamedAcceleration {
    eph::Body body;
    Vec3 a_m_s2;
};

class Attraction {
public:
    Attraction() = delete;

    /// The sum over `bodies`.  Positions are obtained from `eph` relative to the
    /// EARTH, in one call per body, rather than by differencing two barycentric
    /// vectors: at the Moon that would subtract two 1.5e8 km quantities to get a
    /// 3.8e5 km one and throw away three digits for nothing.
    [[nodiscard]] static odl::Result<frames::Acceleration<frames::Frame::GCRS>, ThirdBodyError>
    acceleration(const frames::Position<frames::Frame::GCRS>& sat,
                 const std::vector<eph::Body>& bodies,
                 const eph::Ephemeris& ephemeris,
                 const GravitationalParameters& gm,
                 const odl::time::Epoch& when,
                 const odl::time::LeapTable& leaps);

    [[nodiscard]] static odl::Result<std::vector<NamedAcceleration>, ThirdBodyError>
    by_body(const frames::Position<frames::Frame::GCRS>& sat,
            const std::vector<eph::Body>& bodies,
            const eph::Ephemeris& ephemeris,
            const GravitationalParameters& gm,
            const odl::time::Epoch& when,
            const odl::time::LeapTable& leaps);

    /// The expression as written, direct minus indirect.  Metres in, m/s^2 out.
    [[nodiscard]] static Vec3 pair_direct(const Vec3& sat_m, const Vec3& body_m,
                                          double gm_m3_s2) noexcept;

    /// The same quantity rearranged so that no near-equal vectors are
    /// differenced.  Derived in the source rather than cited; PERT-A-015
    /// measures whether the difference is worth having before it is adopted.
    [[nodiscard]] static Vec3 pair_stable(const Vec3& sat_m, const Vec3& body_m,
                                          double gm_m3_s2) noexcept;
};

}  // namespace odl::thirdbody
