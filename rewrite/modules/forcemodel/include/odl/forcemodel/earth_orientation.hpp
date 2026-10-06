#pragma once
// odl/forcemodel/earth_orientation.hpp — where an ITRS plugin gets its rotation, and how long a record it holds is stated
// (SPEC-forcemodel.md §3.3, finding F4).
//
// L4's `Drag` holds ONE `EopRecord`; for an arc of days the rotation, UT1 and polar motion must be looked up per epoch.  The plugins of
// this module therefore take an EarthOrientation, which is EITHER a series (queried per call, with a stated policy) OR a fixed record, and
// each says which it was built with.

#include <odl/core/result.hpp>
#include <odl/eop/record.hpp>
#include <odl/eop/series.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/frames/transform.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <optional>
#include <string>

namespace odl::forcemodel {

class EarthOrientation {
public:
    /// The series and the leap table are borrowed and must outlive this value.
    [[nodiscard]] static EarthOrientation from_series(const eop::EopSeries& series, eop::EopPolicy policy,
                                                      const time::LeapTable& leaps) {
        return EarthOrientation{&series, policy, std::nullopt, &leaps};
    }
    [[nodiscard]] static EarthOrientation from_record(eop::EopRecord record, const time::LeapTable& leaps) {
        return EarthOrientation{nullptr, eop::EopPolicy{}, record, &leaps};
    }

    /// Both outputs of one lookup: the record (polar motion and UT1 − UTC, which the tides also need) and the rotation built from it.
    struct At {
        eop::EopRecord record;
        frames::Rotation rotation;
    };
    [[nodiscard]] odl::Result<At, ForceModelError> at(const time::Epoch& t) const;

    /// "series" or "fixed record" — the statement each plugin makes (finding F4).
    [[nodiscard]] std::string describe() const { return series_ ? "series" : "fixed record"; }
    [[nodiscard]] const time::LeapTable& leaps() const noexcept { return *leaps_; }

private:
    EarthOrientation(const eop::EopSeries* s, eop::EopPolicy p, std::optional<eop::EopRecord> fixed, const time::LeapTable* l)
        : series_(s), policy_(p), fixed_(std::move(fixed)), leaps_(l) {}

    const eop::EopSeries* series_;
    eop::EopPolicy policy_;
    std::optional<eop::EopRecord> fixed_;
    const time::LeapTable* leaps_;
};

}  // namespace odl::forcemodel
