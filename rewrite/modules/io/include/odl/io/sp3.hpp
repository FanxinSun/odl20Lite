#pragma once
// odl/io/sp3.hpp — the Extended Standard Product 3 orbit format (SP3-d), as read.
//
// SPEC-io-formats.md §3.2. Source: Hilla, S. (NGS/NOAA), "The Extended Standard
// Product 3 Orbit Format (SP3-d)", 21 February 2016 (`SP3D`, files.igs.org/pub/
// data/format/sp3d.pdf). Every field below is read at the column position `SP3D`
// states, fixed-column, not whitespace-delimited.
//
// This is the tree's ONE SP3 reader (SPEC-io-formats.md §1): every tool under
// tools/ that previously parsed SP3 ad hoc reads through this module instead.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/time_scale.hpp>

#include <optional>
#include <string>
#include <vector>

namespace odl::io {

using Sp3Error = odl::Diagnostic;

/// `SP3D`'s own seven Time System codes (`%c` line 1, columns 10-12).
enum class Sp3TimeSystem { GPS, GLO, GAL, BDT, TAI, QZS, UTC };

/// Maps a `%c`-line time system onto this tree's own `odl::time::TimeScale`, a
/// pure function needing no `LeapTable` (SPEC-io-formats.md §3.1). GPS/TAI/UTC
/// map directly; GLONASS/Galileo/BeiDou/QZSS system time each have their own
/// epoch and/or a small time-varying offset from GPS time that `TimeScale` has
/// no variant for, so this refuses (`IOFM-F-003`) rather than approximating.
[[nodiscard]] odl::Result<time::TimeScale, Sp3Error> to_time_scale(Sp3TimeSystem s);

/// The first header line's own column-3 flag: whether this file's own first
/// epoch is position-only or carries velocity too. A later epoch may still
/// carry `V`/`EV` records even when this is `Position` (`SP3D` Example 2's own
/// header says `V` throughout, since the flag names the FILE, not any one
/// epoch) -- callers read each record's own `velocity` field, never this flag,
/// to know whether a given epoch has velocity.
enum class Sp3PosVelFlag { Position, Velocity };

/// SP3 line one and two, and the satellite-id / accuracy tables (lines three
/// onward, however many `SP3D` line 3's own `Number of Sats` field requires).
struct Sp3Header {
    Sp3PosVelFlag pos_vel_flag;
    time::Calendar start_epoch;       ///< line 1, columns 4-31
    int num_epochs = 0;               ///< line 1, columns 33-39
    std::string data_used;            ///< line 1, columns 41-45 ("ORBIT" etc.)
    std::string coordinate_sys;       ///< line 1, columns 47-51 ("WGS84" etc.)
    std::string orbit_type;           ///< line 1, columns 53-55
    std::string agency;               ///< line 1, columns 57-60

    int gps_week = 0;                 ///< line 2, columns 4-7
    double seconds_of_week = 0.0;     ///< line 2, columns 9-23
    /// line 2, columns 25-38 -- THE FIELD THE PREDECESSOR READ WRONG
    /// (`IOFM-R-002`/`IOFM-A-001`): it is NOT derivable from the GPS week and
    /// seconds-of-week pair that precede it on the same line, and reading the
    /// wrong column there is exactly the historical defect this reader exists
    /// to make structurally impossible to repeat.
    double epoch_interval_s = 0.0;
    int mod_jul_day_start = 0;        ///< line 2, columns 40-44
    double fractional_day = 0.0;      ///< line 2, columns 46-60

    /// One entry per satellite named in `Number of Sats`, e.g. "G01". Order
    /// matches the file's own printed order (`SP3D` lines three onward).
    std::vector<std::string> satellite_ids;
    /// Parallel to `satellite_ids` -- one accuracy exponent per satellite
    /// (`SP3D`'s own "++" lines, base^n mm).
    std::vector<int> accuracy;

    std::string file_type;             ///< `%c` line 1, columns 4-5
    /// `%c` line 1, columns 7-8 (reserved, no name given). NOT trimmed on
    /// read, unlike `file_type` -- a caller constructing an `Sp3Header` by
    /// hand and leaving this empty will see it come back as two literal
    /// spaces after a write/read round trip (the column is fixed-width and
    /// this field is opaque, so there is no trim rule to apply consistently);
    /// a real file's own value here is never truly empty either.
    std::string c1_reserved_2char;
    Sp3TimeSystem time_system = Sp3TimeSystem::GPS;  ///< `%c` line 1, columns 10-12
    /// `SP3D` documents the two `%c` lines' own remaining columns, both `%f`
    /// lines and both `%i` lines only by field WIDTH, assigning no semantic
    /// name (every real file fills them with placeholder characters or
    /// zeroes) -- preserved verbatim for round-trip fidelity; this reader
    /// interprets none of them.
    std::string c1_trailer;           ///< `%c` line 1, columns 13-60 (after time_system)
    std::string c2_line;              ///< `%c` line 2, columns 3-60
    std::string f1_line, f2_line;     ///< `%f` lines, columns 3-60
    std::string i1_line, i2_line;     ///< `%i` lines, columns 3-60
    std::vector<std::string> comments;               ///< `/*` lines, verbatim
};

/// The Position and Clock Record (`P`) -- `SP3D`'s own "SP3 Line Thirty three".
struct Sp3PositionRecord {
    std::string satellite_id;
    double x_km = 0.0, y_km = 0.0, z_km = 0.0;
    double clock_us = 0.0;
    /// Standard-deviation EXPONENTS (base^n mm / base^n psec, `SP3D`'s own
    /// units), absent when the source prints blank columns 62-73.
    std::optional<int> x_sdev, y_sdev, z_sdev, clock_sdev;
    bool clock_event = false;   ///< column 75, 'E'
    bool clock_pred = false;    ///< column 76, 'P'
    bool maneuver = false;      ///< column 79, 'M'
    bool orbit_pred = false;    ///< column 80, 'P'
};

/// The Position and Clock Correlation Record (`EP`), optional, one per `P`.
struct Sp3PositionCorrelation {
    int x_sdev_mm = 0, y_sdev_mm = 0, z_sdev_mm = 0;
    int clock_sdev_psec = 0;
    long xy_correlation = 0, xz_correlation = 0, xc_correlation = 0;
    long yz_correlation = 0, yc_correlation = 0, zc_correlation = 0;
};

/// The Velocity and Clock Rate-of-Change Record (`V`), optional, one per `P`.
struct Sp3VelocityRecord {
    std::string satellite_id;
    double x_dm_s = 0.0, y_dm_s = 0.0, z_dm_s = 0.0;
    double clock_rate = 0.0;   ///< 1e-4 microsec/sec
    std::optional<int> x_sdev, y_sdev, z_sdev, clock_rate_sdev;
};

/// The Velocity and Clock Rate-of-Change Correlation Record (`EV`), optional.
struct Sp3VelocityCorrelation {
    int x_sdev = 0, y_sdev = 0, z_sdev = 0;   ///< 1e-4 mm/sec
    int clock_rate_sdev = 0;                   ///< 1e-4 psec/sec
    long xy_correlation = 0, xz_correlation = 0, xc_correlation = 0;
    long yz_correlation = 0, yc_correlation = 0, zc_correlation = 0;
};

/// One satellite's own full record set at one epoch: `P` always present,
/// `EP`/`V`/`EV` each present only if the file carries them.
struct Sp3SatelliteRecord {
    Sp3PositionRecord position;
    std::optional<Sp3PositionCorrelation> position_correlation;
    std::optional<Sp3VelocityRecord> velocity;
    std::optional<Sp3VelocityCorrelation> velocity_correlation;
};

/// One Epoch Header Record (`*`) and every satellite's own record that follows
/// it, up to the next `*` record or `EOF`.
struct Sp3Epoch {
    time::Calendar epoch;
    std::vector<Sp3SatelliteRecord> satellites;
};

struct Sp3File {
    Sp3Header header;
    std::vector<Sp3Epoch> epochs;
};

/// Reads an SP3-d file already in memory (no file access here, SPEC-io-formats.md
/// §3.1). Header records are found by their own leading marker (`IOFM-R-001`):
/// `+` satellite-id lines read until the first `++`, `++` accuracy lines read
/// until the first `%c`, comment lines read until the first `*` -- never a fixed
/// line count, which is the predecessor's own second defect this reader does not
/// repeat.
[[nodiscard]] odl::Result<Sp3File, Sp3Error> read_sp3(std::string_view text);

/// Serialises an `Sp3File` back to SP3-d text. Round-trip is SEMANTIC
/// (`read_sp3(write_sp3(f)) == f`, field by field), not a byte-identical
/// reproduction of any particular real file's own whitespace choices.
[[nodiscard]] odl::Result<std::string, Sp3Error> write_sp3(const Sp3File& file);

[[nodiscard]] bool operator==(const Sp3Header& a, const Sp3Header& b) noexcept;
[[nodiscard]] bool operator==(const Sp3PositionRecord& a, const Sp3PositionRecord& b) noexcept;
[[nodiscard]] bool operator==(const Sp3PositionCorrelation& a, const Sp3PositionCorrelation& b) noexcept;
[[nodiscard]] bool operator==(const Sp3VelocityRecord& a, const Sp3VelocityRecord& b) noexcept;
[[nodiscard]] bool operator==(const Sp3VelocityCorrelation& a, const Sp3VelocityCorrelation& b) noexcept;
[[nodiscard]] bool operator==(const Sp3SatelliteRecord& a, const Sp3SatelliteRecord& b) noexcept;
[[nodiscard]] bool operator==(const Sp3Epoch& a, const Sp3Epoch& b) noexcept;
[[nodiscard]] bool operator==(const Sp3File& a, const Sp3File& b) noexcept;

}  // namespace odl::io
