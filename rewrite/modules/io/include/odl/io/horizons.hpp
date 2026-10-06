#pragma once
// odl/io/horizons.hpp — a JPL Horizons vector-table response, as read.
//
// SPEC-io-horizons.md §3. Source: Solar System Dynamics Group, JPL (NASA/
// Caltech), Horizons API Documentation (`HZAPI`, ssd-api.jpl.nasa.gov/doc/
// horizons.html), verified directly against a real query's own output.
//
// This is the tree's ONE Horizons reader: it parses already-fetched text
// (no file or network access here, matching every reader in odl::io) for a
// VECTOR table only (EPHEM_TYPE=VECTORS, VEC_TABLE=2, VEC_CORR=NONE,
// REF_SYSTEM=ICRF, TIME_TYPE=TDB, OUT_UNITS=KM-S) -- observer and element
// tables are out of scope (SPEC-io-horizons.md §1).

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/time_scale.hpp>

#include <string>
#include <vector>

namespace odl::io {

using HorizonsError = odl::Diagnostic;

/// The time system a vector-table record is printed in. `HZAPI` names both
/// as valid for a vector table, but the client (SPEC-io-horizons.md §5)
/// always requests `TIME_TYPE=TDB` explicitly -- `Ut` exists only so a
/// response that does not honour that request is a recognised, named
/// refusal (`IOHZ-F-002`) rather than an unparseable token.
enum class HorizonsTimeSystem { Tdb, Ut };

/// Pure mapping, no `LeapTable` needed (§3.1) -- `Tdb` succeeds; `Ut`
/// refuses, since this reader never requests it and `HZAPI` itself does not
/// state exactly what "UT" denotes at this API boundary (UT1, or a UTC-like
/// civil time).
[[nodiscard]] odl::Result<time::TimeScale, HorizonsError> to_time_scale(HorizonsTimeSystem s);

/// One state-vector record: the epoch as printed (a raw `Calendar`, no
/// `Epoch` constructed here -- see sp3.hpp's own header for why), the
/// time system that epoch was printed in, geocentric ICRF position (km)
/// and velocity (km/s).
struct HorizonsStateRecord {
    time::Calendar epoch;
    HorizonsTimeSystem time_system = HorizonsTimeSystem::Tdb;
    odl::Vec3 position_km;
    odl::Vec3 velocity_km_s;
};

/// A parsed vector-table response. `target_body`/`center_body` are the
/// header's own `Target body name`/`Center body name` lines, carried
/// verbatim (not parsed further) so a caller can at least log which object
/// and center a table was actually for.
struct HorizonsEphemeris {
    std::string target_body;
    std::string center_body;
    std::vector<HorizonsStateRecord> states;
};

/// Reads an already-fetched Horizons vector-table response (no file or
/// network access here, SPEC-io-horizons.md §1). The header's own `Output
/// units`, `Reference frame` and `Output type` lines are checked
/// (`IOHZ-R-003`) before any record is trusted; every record's own printed
/// time-system token is checked independently (`IOHZ-R-002`); records are
/// found between `$$SOE`/`$$EOE`, never by a fixed count (`IOHZ-R-001`).
[[nodiscard]] odl::Result<HorizonsEphemeris, HorizonsError> read_horizons(std::string_view text);

/// Serialises a vector table in the layout `read_horizons` reads (`IOHZ-R-004`): the header's body names and the three checked lines (`KM-S`, `GEOMETRIC cartesian states`, `ICRF`), the
/// `$$SOE`/`$$EOE` sentinels and three lines per record. The round trip is SEMANTIC, as `write_sp3`'s is: `read_horizons(write_horizons(e)) == e` to the last bit of every position and
/// velocity (17 significant digits are written, Horizons' own 16 would not hold an arbitrary double), not a byte-identical copy of any real response — the Julian date printed on a record's
/// first line is informational (the reader reads past it, `SPEC-io-horizons.md` §3.3) and the stop/step/EOP lines of a real header are not reproduced. `IOHZ-F-002` for a record in any time
/// system but TDB (the reader would refuse it); `IOHZ-F-006` for a component that is not finite or a body name that holds a line break.
[[nodiscard]] odl::Result<std::string, HorizonsError> write_horizons(const HorizonsEphemeris& eph);

[[nodiscard]] bool operator==(const HorizonsStateRecord& a, const HorizonsStateRecord& b) noexcept;
[[nodiscard]] bool operator==(const HorizonsEphemeris& a, const HorizonsEphemeris& b) noexcept;

}  // namespace odl::io
