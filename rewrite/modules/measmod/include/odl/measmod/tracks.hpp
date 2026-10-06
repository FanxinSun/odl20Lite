#pragma once
// odl/measmod/tracks.hpp — the four abstractions through which the models reach the world.
//
// SPEC-measmod.md §5.1, MEAS-R-064: the target, the station, the Earth's orientation and the Earth's motion are reached
// through four small abstractions, so that a closed-form test can replace any of them and so that the models never assume the
// target is Keplerian or the station Earth-fixed. Everything a station or the Earth reports is in GCRS axes and in metres,
// seconds and radians; the target's state is the frames module's own typed `GcrsState` (kilometres, kilometres per second),
// the one place a unit crossing happens, through `odl::metres_from_km`.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/state.hpp>
#include <odl/measmod/registry.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

namespace odl::measmod {

/// The target's centre of mass, TT-compatible GCRS, as a typed state (km, km/s). May refuse (an epoch outside what it holds).
class Trajectory {
public:
    virtual ~Trajectory() = default;
    [[nodiscard]] virtual odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const = 0;
};

/// One reference point of a station at an epoch, GCRS axes, metres and seconds.
struct StationKinematics {
    odl::Vec3 position_m;
    odl::Vec3 velocity_m_s;          ///< inertial
    odl::Vec3 up;                    ///< the unit vector along the ellipsoid normal at the station
    odl::Vec3 up_rate_per_s;         ///< dû/dt
};

class StationTrack {
public:
    virtual ~StationTrack() = default;
    [[nodiscard]] virtual odl::Result<StationKinematics, MeasError> at(const odl::time::Epoch& when) const = 0;
};

/// The rotation GCRS → ITRS and the Earth's rotation vector in GCRS axes at an epoch.
struct EarthOrientationSample {
    odl::Mat3 gcrs_to_itrs;
    odl::Vec3 omega_gcrs_rad_s;
};

class EarthOrientation {
public:
    virtual ~EarthOrientation() = default;
    [[nodiscard]] virtual odl::Result<EarthOrientationSample, MeasError> at(const odl::time::Epoch& when) const = 0;
};

/// The geocentre's barycentric position and velocity in the GCRS axes (the BCRS axes: the same orientation), metres and m/s.
struct EarthMotionSample {
    odl::Vec3 position_m;
    odl::Vec3 velocity_m_s;
};

class EarthMotion {
public:
    virtual ~EarthMotion() = default;
    [[nodiscard]] virtual odl::Result<EarthMotionSample, MeasError> at(const odl::time::Epoch& when) const = 0;
};

// ---- the production implementations ----------------------------------------------------------------------------------------

/// `frames::gcrs_to_itrs` with the EOP of `series` (policy applied). Non-owning: the series and the leap table must outlive it.
/// A refusal of the EOP series or of the frames chain is returned with its own id.
class EopEarthOrientation final : public EarthOrientation {
public:
    EopEarthOrientation(const odl::eop::EopSeries& series, odl::eop::EopPolicy policy, const odl::time::LeapTable& leaps) noexcept
        : series_(&series), policy_(policy), leaps_(&leaps) {}
    [[nodiscard]] odl::Result<EarthOrientationSample, MeasError> at(const odl::time::Epoch& when) const override;

private:
    const odl::eop::EopSeries* series_;
    odl::eop::EopPolicy policy_;
    const odl::time::LeapTable* leaps_;
};

/// A station fixed to the Earth: the position is the ITRS point rotated to GCRS at the epoch, the velocity is the transport term
/// ω × s the frames module already forms, the vertical is the ellipsoid normal at the point (its geodetic latitude and
/// longitude are the caller's, from the registry) rotated to GCRS, and dû/dt = ω × û. Non-owning of the orientation.
class EarthFixedStation final : public StationTrack {
public:
    EarthFixedStation(odl::Vec3 itrs_position_m, Geodetic geodetic, const EarthOrientation& orientation) noexcept
        : itrs_position_m_(itrs_position_m), geodetic_(geodetic), orientation_(&orientation) {}
    [[nodiscard]] odl::Result<StationKinematics, MeasError> at(const odl::time::Epoch& when) const override;

private:
    odl::Vec3 itrs_position_m_;
    Geodetic geodetic_;
    const EarthOrientation* orientation_;
};

/// The Earth's barycentric state from the planetary ephemeris. Non-owning of the ephemeris and the leap table.
class EphemerisEarthMotion final : public EarthMotion {
public:
    EphemerisEarthMotion(const odl::eph::Ephemeris& ephemeris, const odl::time::LeapTable& leaps) noexcept
        : ephemeris_(&ephemeris), leaps_(&leaps) {}
    [[nodiscard]] odl::Result<EarthMotionSample, MeasError> at(const odl::time::Epoch& when) const override;

private:
    const odl::eph::Ephemeris* ephemeris_;
    const odl::time::LeapTable* leaps_;
};

}  // namespace odl::measmod
