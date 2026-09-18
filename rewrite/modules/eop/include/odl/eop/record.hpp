#pragma once
// odl/eop/record.hpp — one epoch's Earth orientation, in canonical units.
//
// SPEC-eop.md EOP-R-010: radians for every angle and angular rate, seconds for
// every time.  No file's native units propagate past the parser, because the two
// products this tree reads disagree about units in two fields by a factor of a
// thousand: EOP 20 C04 gives dX, dY in ARCSECONDS and LOD in SECONDS, while
// finals2000A gives them in MILLIARCSECONDS and MILLISECONDS.  A value converted
// with the wrong product's factor is still a plausible-looking EOP.

#include <algorithm>
#include <optional>
#include <string>

namespace odl::eop {

/// Where a value came from, worst-first.  Per QUANTITY, not per epoch: the
/// products flag polar motion, UT1 and nutation separately and a row can be
/// observed in one and predicted in another.
enum class Quality { Final, BulletinB, Rapid, Predicted };

[[nodiscard]] constexpr const char* name_of(Quality q) noexcept {
    switch (q) {
        case Quality::Final:      return "final";
        case Quality::BulletinB:  return "BulletinB";
        case Quality::Rapid:      return "rapid";
        case Quality::Predicted:  return "PREDICTED";
    }
    return "?";
}

struct EopQuality {
    Quality pole = Quality::Final;
    Quality ut1 = Quality::Final;
    Quality nutation = Quality::Final;

    [[nodiscard]] Quality worst() const noexcept {
        return std::max({pole, ut1, nutation});
    }
};

struct EopRecord {
    double xp = 0.0;      ///< pole coordinate, radians
    double yp = 0.0;      ///< pole coordinate, radians
    double dut1 = 0.0;    ///< UT1 − UTC, seconds
    double lod = 0.0;     ///< excess length of day, seconds
    double dx = 0.0;      ///< CIP offset, radians
    double dy = 0.0;      ///< CIP offset, radians
    std::optional<double> xrt;   ///< pole rate, radians per second (C04 only)
    std::optional<double> yrt;

    EopQuality quality{};

    /// EOP-R-045.  The published series are REGULARISED: the sub-daily ocean-tide
    /// and libration signals have been removed and must be added back to obtain
    /// the instantaneous orientation.  `frames` refuses a record for which they
    /// have not been (FRAME-F-003), because the raw values are legitimately
    /// wanted when comparing against IERS daily values and are wrong for the one
    /// use that matters.
    bool subdaily_applied = false;

    /// EOP-R-028.  finals2000A's dX, dY have free core nutation NOT removed.
    /// Whether that is wanted depends on whether the consumer also applies an
    /// FCN model; applying both double-counts 0.1-0.3 mas, i.e. 3-10 mm at LEO.
    bool fcn_removed = false;
};

/// How much a caller will tolerate.  Passed per call rather than stored in the
/// series, so one process can serve a planning query that accepts predictions
/// and a campaign query that does not, from the same loaded data.
struct EopPolicy {
    /// Worst acceptable provenance.  A campaign run sets this to BulletinB or
    /// better; Predicted is legitimate for planning and illegitimate for a
    /// published fit, and the difference must be a declared choice.
    Quality max_quality = Quality::Rapid;
    bool apply_subdaily = true;

    double splice_pole_limit_rad = 4.8481368110953599e-9;   // 1 mas
    double splice_ut1_limit_s = 1.0e-4;                     // 0.1 ms

    /// EOP-R-012.  Bounded for the historical era only, and configurable,
    /// because CGPM Resolution 4 (2022) will raise the permitted maximum when
    /// leap seconds stop.  Hard-coding 0.9 s would make this module fail on data
    /// it will be given.
    double dut1_range_limit_s = 1.0;
};

struct EopProvenance {
    std::string source_id;
    std::string product;     ///< "EOP 20 C04" or "finals2000A"
    std::string header_model; ///< the file's own model line, recorded as found
    std::string note;
};

}  // namespace odl::eop
