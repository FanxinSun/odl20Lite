#pragma once
// odl/measmod/partials.hpp — a partials row typed by the frame of the state it differentiates.
//
// SPEC-measmod.md MEAS-R-060, -R-062: every modelled value carries a COMPLETE partials row with respect to the target's inertial
// state (r, v) at the nominal bounce, emission or observation epoch, GCRS axes, all in SI (metres per metre and per metre per second,
// radians per metre and per metre per second). The velocity columns are zero to first order for a range or an angle and the row is
// analytic wherever it is derivable (MEAS-R-061).

#include <odl/frames/state.hpp>

#include <array>
#include <cstddef>

namespace odl::measmod {

template <odl::frames::Frame F, std::size_t Rows>
struct Partials {
    static constexpr odl::frames::Frame frame = F;
    static constexpr std::size_t rows = Rows;
    /// d[i] is the i-th observation component's derivative with respect to (x, y, z, vx, vy, vz) of the state in frame F.
    std::array<std::array<double, 6>, Rows> d{};
};

}  // namespace odl::measmod
