#pragma once
// odl/measmod/position.hpp — the ephemeris-position observation and its model.
//
// SPEC-measmod.md §4.6 (MEAS-R-040 … -R-042), §5.3, §5.4, §6.4, §7 (MEAS-F-014, -F-018), §8.6.
//
// An SP3 position and a Horizons vector with VEC_CORR = NONE are the target's GEOMETRIC position at the epoch — no light time, no aberration —
// in the observation's own frame: GCRS for a Horizons ICRF table (the two axes are the same to the model's precision), ITRS for an SP3 file.
// The model rotates the target's GCRS position to that frame at the epoch and its partials row is [M 0], M the rotation (the identity in GCRS).
// The observation epoch is the same in every evaluation of a perturbed trajectory, so no Earth-orientation floor enters a finite difference of it.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/io/horizons.hpp>
#include <odl/io/sp3.hpp>
#include <odl/measmod/partials.hpp>
#include <odl/measmod/range.hpp>
#include <odl/measmod/tracks.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <string>
#include <vector>

namespace odl::measmod {

/// The frame a position observation is expressed in.
enum class PositionFrame { GCRS, ITRS };

/// Which point of the satellite a position refers to (MEAS-R-042): `SP3D` is silent on it, so the caller says. Only the centre of mass is modelled.
enum class ReferencePoint { CentreOfMass, AntennaPhaseCentre, Unspecified };

/// A position at an epoch in a frame, metres. A plain aggregate: the builders resolve a tag's time scale and refuse what the model cannot take, but they are
/// not the only way to hold one (MEAS-A-060 builds a defective epoch on purpose, to show the identities detect it).
struct PositionObservation {
    odl::time::Epoch epoch;
    PositionFrame frame = PositionFrame::GCRS;
    odl::Vec3 position_m;
    ReferencePoint point = ReferencePoint::CentreOfMass;
};

/// MEAS-R-041: the SP3 coordinate-system codes the table maps to the ITRS, in the order the specification lists them.
[[nodiscard]] const std::vector<std::string>& accepted_sp3_frame_codes();

/// An SP3 position record at the epoch `epoch` of the epoch header that holds it, in the header's time system, as an ITRS observation. MEAS-F-014: a time system other
/// than GPS, TAI or UTC, a coordinate-system code that is not in the table (compared exactly, after the trailing blanks of the field), a reference point that is not the centre of mass;
/// MEAS-F-018: a component that is not finite, or the format's marker for an absent position (all three components exactly 0). The time module's own refusal of a calendar is returned
/// with its own id. Positions are kilometres in the file and metres in the observation, by the tree's one crossing, `odl::metres_from_km`.
[[nodiscard]] odl::Result<PositionObservation, MeasError> position_observation(const odl::io::Sp3Header& header, const odl::time::Calendar& epoch,
                                                                               const odl::io::Sp3PositionRecord& record, ReferencePoint point,
                                                                               const odl::time::LeapTable& leaps);

/// A Horizons vector-table record as a GCRS observation at its TDB epoch (MEAS-R-010). A time system other than TDB refuses with the reader's own id (IOHZ-F-002), unchanged;
/// MEAS-F-014 for a reference point that is not the centre of mass; MEAS-F-018 for a component that is not finite.
[[nodiscard]] odl::Result<PositionObservation, MeasError> position_observation(const odl::io::HorizonsStateRecord& record, ReferencePoint point,
                                                                               const odl::time::LeapTable& leaps);

/// What a modelled position applied and what it did not (MEAS-R-038's list for a position).
struct PositionApplied {
    PositionFrame frame = PositionFrame::GCRS;
    odl::Mat3 rotation = odl::Mat3::identity();       ///< M: GCRS → the observation's frame at the epoch
    std::vector<OmittedTerm> omitted;                 ///< the ITRS realisation differences, for an ITRS observation
};

/// The model's answer. Only `model_position` builds one.
class ModelledPosition {
public:
    [[nodiscard]] const odl::Vec3& position_m() const noexcept { return position_m_; }          ///< the target's geometric position in the observation's frame
    [[nodiscard]] const odl::Vec3& observed_m() const noexcept { return observed_m_; }
    [[nodiscard]] odl::Vec3 residual_m() const noexcept { return observed_m_ - position_m_; }   ///< observed − modelled, in the observation's frame
    [[nodiscard]] const odl::time::Epoch& epoch() const noexcept { return epoch_; }
    /// Rows: the components of the observation's frame; columns (x, y, z, vx, vy, vz) of the target's GCRS state at the epoch: [M 0].
    [[nodiscard]] const Partials<odl::frames::Frame::GCRS, 3>& partials() const noexcept { return partials_; }
    [[nodiscard]] const PositionApplied& applied() const noexcept { return applied_; }

private:
    friend odl::Result<ModelledPosition, MeasError> model_position(const PositionObservation&, const Trajectory&, const EarthOrientation&);
    ModelledPosition(odl::Vec3 modelled, odl::Vec3 observed, odl::time::Epoch epoch, Partials<odl::frames::Frame::GCRS, 3> partials, PositionApplied applied)
        : position_m_(modelled), observed_m_(observed), epoch_(epoch), partials_(partials), applied_(std::move(applied)) {}
    odl::Vec3 position_m_, observed_m_;
    odl::time::Epoch epoch_;
    Partials<odl::frames::Frame::GCRS, 3> partials_;
    PositionApplied applied_;
};

/// MEAS-R-040: the geometric position of `target` at the observation epoch, in the observation's frame. A refusal of the trajectory or of the Earth's orientation (needed for an
/// ITRS observation only) is returned with its own id and this module's context.
[[nodiscard]] odl::Result<ModelledPosition, MeasError> model_position(const PositionObservation& obs, const Trajectory& target, const EarthOrientation& earth);

}  // namespace odl::measmod
