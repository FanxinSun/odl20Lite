#pragma once
// odl/measmod/registry.hpp — the station and site registry.
//
// SPEC-measmod.md §4.1 (MEAS-R-001 … -R-007), §7 (MEAS-F-001 … -F-006, -F-020, -F-024), §8.1.
//
// SLR stations come from SLRF2020 (the marker positions and velocities) and the ILRS eccentricity file
// (marker -> system reference point, in up/north/east), joined on the SOD, with the ITRF2020 post-seismic
// event list read only to REFUSE a site after one of its events. Optical sites are supplied by the caller,
// with a citation. Nothing is bundled and nothing is fetched: this module reads text the caller already
// holds (the manifest's cache), and records the hashes it was told about so a run's provenance can name them.
//
// The coordinates are of the geodetic MARKERS, not of the system reference point; the eccentricity gives
// that offset (the pinned ILRS file: "from the marker to the intersection of optical axis"). The marker's
// position is a LINEAR model, which is wrong after a post-seismic event: the registry refuses such a site,
// with exactly one named override, recorded in what it returns (SPEC-template.md R-ERR-3).

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/io/sinex.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace odl::measmod {

using MeasError = odl::Diagnostic;

/// A CDP pad number: four digits naming a place, not a station (several systems occupy a pad).
/// Distinct from SodKey and OpticalStationNumber: a bare four-digit number does not say which
/// namespace it is in (plan §5 constraint 10, MEAS-R-004).
struct PadId {
    int value = 0;
    constexpr explicit PadId(int v) noexcept : value(v) {}
    friend constexpr bool operator==(PadId, PadId) noexcept = default;
};

/// A station as the ILRS names it: the pad, the system number and the system's occupancy of the pad.
/// The SOD of the files is pad × 10⁴ + system × 10² + occupancy; the CRD `H2` record carries the three
/// parts (the real file's `h2 YARL 7090 5 13 3` is 70900513).
struct SodKey {
    int pad = 0;
    int system = 0;
    int occupancy = 0;

    constexpr SodKey() noexcept = default;
    /// Three parts named, never one number: use `make` (which refuses what cannot be a SOD) or `from_sod`.
    constexpr SodKey(int pad_, int system_, int occupancy_) noexcept : pad(pad_), system(system_), occupancy(occupancy_) {}

    /// Refuses a part outside the SOD's own field widths (a pad of 1…9999, a system and an occupancy of
    /// 0…99) with MEAS-F-001: a number that cannot be a SOD is not a station.
    [[nodiscard]] static odl::Result<SodKey, MeasError> make(int pad, int system, int occupancy);
    /// From the eight-digit SOD of the files.
    [[nodiscard]] static odl::Result<SodKey, MeasError> from_sod(long sod);

    [[nodiscard]] constexpr long sod() const noexcept {
        return static_cast<long>(pad) * 10000L + static_cast<long>(system) * 100L + occupancy;
    }
    friend constexpr bool operator==(const SodKey&, const SodKey&) noexcept = default;
};

/// The number an IOD line carries in columns 17–20 (`IODFMT`): an observer's own station, in a namespace
/// of its own. Distinct from PadId.
struct OpticalStationNumber {
    int value = 0;
    constexpr explicit OpticalStationNumber(int v) noexcept : value(v) {}
    friend constexpr bool operator==(OpticalStationNumber, OpticalStationNumber) noexcept = default;
};

/// A point on the WGS 84 ellipsoid: geodetic latitude and longitude (radians) and ellipsoidal height (metres).
struct Geodetic {
    double latitude_rad = 0.0;
    double longitude_rad = 0.0;
    double height_m = 0.0;
};

/// A SINEX epoch `YY:DDD:SSSSS` decoded but not yet put in a time scale (SPEC-measmod.md §3.6): two-digit years 00–49 are
/// 20YY and 50–99 are 19YY (the files' data run from 1976 to 2026), the day of year counts from 1, the seconds are those
/// of the UTC day. `00:000:00000` is "no epoch": unknown as a start, open as an end.
struct SinexTime {
    bool unset = false;
    int year = 0;
    int day_of_year = 0;
    int seconds_of_day = 0;
};

/// MEAS-F-020 for text that is not `YY:DDD:SSSSS`, a day that the year does not have, or seconds above 86 399.
[[nodiscard]] odl::Result<SinexTime, MeasError> decode_sinex_epoch(std::string_view text);

/// Everything the registry knows of one station at one epoch.
struct SlrSite {
    SodKey sod;
    int pad = 0;
    char point = ' ';                 ///< the SINEX point code of the marker ('A', 'B', …)
    std::string domes;                ///< the marker's DOMES number
    std::string name;                 ///< the description column of SITE/ID, trimmed
    odl::Vec3 marker_itrs_m;          ///< x_ref + v (t − t_ref), ITRS, metres
    odl::Vec3 srp_itrs_m;             ///< the system reference point: marker + E (U, N, E)ᵀ
    odl::Vec3 eccentricity_une_m;     ///< up, north, east, metres, from the span containing the epoch
    Geodetic marker_geodetic;
    Geodetic srp_geodetic;
    std::string slrf_release;         ///< the coordinate file's own version text
    std::string ecc_release;          ///< the eccentricity file's own version text
    bool post_seismic_override_used = false;   ///< MEAS-R-005: recorded so a run's provenance carries it
};

/// The per-call options of `site`. There is exactly one override (R-ERR-3): named so that reading it says
/// what safety is given up, set explicitly per call, never by default, recorded in the returned site.
struct SiteOptions {
    bool accept_linear_position_after_post_seismic_event = false;
};

/// The SHA-256 of each document the registry was built from, as the manifest records them (supplied by the
/// caller; this module reads no file and so cannot compute them for itself).
struct RegistrySources {
    std::string slrf_sha256;
    std::string ecc_sha256;
    std::string psd_sha256;
};

/// Immutable once built (MEAS-R-001). Cheap to copy: the tables are shared.
class SlrRegistry {
public:
    /// MEAS-R-001. `psd_text` is the ITRF2020 post-seismic event list, read for its events only. Every
    /// epoch is converted from UTC with `leaps`. Refuses (MEAS-F-020), naming the line, a document that is
    /// malformed or inconsistent.
    [[nodiscard]] static odl::Result<SlrRegistry, MeasError> build(
        const odl::io::SinexFile& slrf, const odl::io::SinexFile& ecc, std::string_view psd_text,
        const odl::time::LeapTable& leaps, RegistrySources sources);

    /// MEAS-R-002 … -R-005. The checks run in this fixed order and the first that fails is returned:
    /// identity (MEAS-F-001), eccentricity span (MEAS-F-002), solution span (MEAS-F-003),
    /// post-seismic (MEAS-F-004). There is no nearest-station or default.
    [[nodiscard]] odl::Result<SlrSite, MeasError> site(SodKey key, const odl::time::Epoch& when,
                                                       SiteOptions options = {}) const;

    /// The SODs of one pad that the registry places; MEAS-F-001 for a pad it does not know.
    [[nodiscard]] odl::Result<std::vector<SodKey>, MeasError> placed_sods_of(PadId pad) const;
    [[nodiscard]] bool knows_pad(PadId pad) const noexcept;

    /// MEAS-R-006: the distinct pads SLRF2020 lists, the SODs the registry places, and the SODs the
    /// eccentricity file knows that it cannot place (no coordinates).
    [[nodiscard]] std::size_t pad_count() const noexcept;
    [[nodiscard]] std::size_t placed_sod_count() const noexcept;
    [[nodiscard]] std::size_t unplaced_sod_count() const noexcept;
    /// The post-seismic events held (site, event) and the sites they concern.
    [[nodiscard]] std::size_t post_seismic_event_count() const noexcept;

    [[nodiscard]] const std::string& slrf_release() const noexcept;
    [[nodiscard]] const std::string& ecc_release() const noexcept;
    [[nodiscard]] const RegistrySources& sources() const noexcept;

    struct Impl;   ///< the tables; defined in the implementation, shared and immutable
private:
    explicit SlrRegistry(std::shared_ptr<const Impl> impl) noexcept : impl_(std::move(impl)) {}
    std::shared_ptr<const Impl> impl_;
};

/// An observer's own site: supplied by the caller with the citation of where its coordinates come from.
struct OpticalSite {
    OpticalStationNumber number;
    Geodetic location;
    std::string citation;
};

/// MEAS-R-007. Empty on construction: no list is bundled (the only public one is GPL-3.0-derived and
/// carries observers' names with coordinates, round-8 report D2).
class OpticalRegistry {
public:
    /// Refuses a blank or whitespace-only citation (MEAS-F-006) and a latitude, longitude or height that
    /// cannot be one (MEAS-F-024); on refusal nothing is added.
    [[nodiscard]] odl::Result<void, MeasError> add(OpticalStationNumber number, double latitude_rad,
                                                   double longitude_rad, double height_m,
                                                   std::string citation);
    /// MEAS-F-005 for a number never added.
    [[nodiscard]] odl::Result<OpticalSite, MeasError> site(OpticalStationNumber number) const;
    [[nodiscard]] std::size_t size() const noexcept { return sites_.size(); }

private:
    std::vector<OpticalSite> sites_;
};

}  // namespace odl::measmod
