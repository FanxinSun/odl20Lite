#pragma once
// odl/atmosphere/atmosphere.hpp — NRLMSISE-00: how much air is there.
//
// SPEC-atmosphere.md.  Three things about this interface are unlike the rest of
// the tree, and each is a ruling rather than a preference.
//
//   * THE REFERENCE IMPLEMENTATION IS THE DEFINITION (ATMO-R-001, plan §4 rule
//     6).  NRL publishes no reference profile, no reference value and no
//     expected output: its test driver publishes 17 fully specified INPUT cases
//     and nothing to compare them against.  The model IS ~1500 fitted
//     coefficients plus the code combining them, both of which exist only in
//     the pinned FORTRAN.  What that costs is stated in SPEC-atmosphere §8:
//     nothing in this tree can check that the FORTRAN computes what the paper
//     describes.
//   * `GTD7` AND `GTD7D` RETURN DIFFERENT QUANTITIES UNDER ONE NAME.  The
//     reference's own header says so in capitals.  They are separate types here
//     and there is no boolean (ATMO-R-004); L4 accepts only the second.
//   * A SPECIES DENSITY CARRIES WHETHER THE MODEL RESOLVES IT (ATMO-R-036).
//     The model's precision was characterised against a DRAG question, and this
//     interface returns nine species densities.  See `SpeciesDensity`.

#include <odl/atmosphere/indices.hpp>
#include <odl/core/result.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace odl::atmosphere {

using AtmoError = odl::Diagnostic;

// --------------------------------------------------------------------------
// Inputs

// `DailyAp` and `ThreeHourlyAp` are in <odl/atmosphere/indices.hpp>, shared with
// the model internals (ATMO-R-009).

/// How a space-weather value was classified by its issuing authority, and
/// whether this tree could verify it against that authority (ATMO-R-022,
/// ATMO-R-031).  Carried into every result that consumed it.
enum class DataClass : std::uint8_t { definitive, kp_definitive, preliminary, predicted };
enum class Verification : std::uint8_t { verified_against_issuer, unverified_redistribution };

/// The three numbers the model takes, with the provenance that makes them
/// reproducible.  Not three loose doubles: a sample knows which pinned snapshot
/// it came from, because revision means two snapshots are not comparable even
/// at an epoch both cover (ATMO-R-024).
struct SpaceWeather {
    double f107_previous_day = 0.0;   ///< OBSERVED flux, day D-1 (ATMO-R-016, -017)
    double f107a_centred81 = 0.0;     ///< computed here, never read from a derived column
    DailyAp daily{};
    ThreeHourlyAp three_hourly{};
    bool has_three_hourly = false;

    std::string snapshot_id;          ///< manifest id
    std::string snapshot_sha256;
    DataClass data_class = DataClass::definitive;
    Verification verification = Verification::unverified_redistribution;
};

// --------------------------------------------------------------------------
// Outputs

/// A species number density that knows whether the model resolves it.
///
/// WHY THIS IS NOT A DOUBLE.  `ATMO-P-1` characterised the reference's precision
/// against the question *can this affect drag*, and answered it with a threshold:
/// a species whose mass contributes less than 1e-15 of the total cannot.  Below
/// that threshold the reference's own arithmetic degrades continuously and then
/// underflows to a HARD ZERO — 18 of 1500 measured comparisons.
///
/// For drag that is the right answer and needs no warning.  For a consumer
/// asking a different question — anomalous oxygen at 110 km for surface erosion,
/// which is a real mechanism and not a hypothetical one — a hard zero returned
/// as a density is a wrong answer with no signal.  The two are different claims:
/// "a small density" and "below what this model resolves".
///
/// So the mass contribution is always available and the number density is a
/// refusal where the model does not resolve it (ATMO-F-017).
class SpeciesDensity {
public:
    constexpr SpeciesDensity() = default;
    constexpr SpeciesDensity(double number_m3, double mass_kg_m3, bool resolved) noexcept
        : number_m3_(number_m3), mass_kg_m3_(mass_kg_m3), resolved_(resolved) {}

    /// Always available: an unresolved species contributes a mass
    /// indistinguishable from zero, which IS the correct answer for drag.
    [[nodiscard]] constexpr double mass_density_kg_m3() const noexcept { return mass_kg_m3_; }

    [[nodiscard]] constexpr bool resolved() const noexcept { return resolved_; }

    /// The number density, m^-3.  Refuses where the model does not resolve it.
    [[nodiscard]] odl::Result<double, AtmoError> number_density_m3() const;

private:
    double number_m3_ = 0.0;
    double mass_kg_m3_ = 0.0;
    bool resolved_ = false;
};

/// The species the model carries, in its own order.
struct Species {
    SpeciesDensity he, o, n2, o2, ar, h, n, anomalous_o;
};

struct Temperatures {
    double exospheric_k = 0.0;   ///< T(1): the global average below 120 km, not a local value
    double at_altitude_k = 0.0;  ///< T(2)
};

/// What was done to the request, recorded so a run can say what it used.
struct EvaluationRecord {
    std::string snapshot_id, snapshot_sha256;
    DataClass data_class = DataClass::definitive;
    Verification verification = Verification::unverified_redistribution;
    bool sub_80km_substitution = false;   ///< ATMO-R-013 forced 150/150/4
    bool year_discarded = true;           ///< the model ignores it (ATMO-R-011)
};

/// `GTD7`'s total mass density: He, O, N2, O2, Ar, H, N.  ANOMALOUS OXYGEN
/// EXCLUDED.  A distinct type from `DragDensity` because the reference gives
/// both the same name and they are not the same number (ATMO-R-004).
struct NeutralDensity {
    Species species{};
    Temperatures temperature{};
    double total_mass_kg_m3 = 0.0;
    EvaluationRecord record{};
};

/// `GTD7D`'s "effective total mass density for drag": the same sum PLUS
/// anomalous oxygen.  **This is the one a drag model wants**, and L4 accepts
/// only this type.
struct DragDensity {
    Species species{};
    Temperatures temperature{};
    double total_mass_kg_m3 = 0.0;
    EvaluationRecord record{};
};

/// Where and when.
///
/// TIME IS A DAY AND SECONDS OF UT, not an `Epoch`, and that is deliberate.
/// NRLMSISE-00 takes a day-of-year and seconds of UT and IGNORES THE YEAR
/// (`ATMO-R-011`); turning an `Epoch` into those needs UTC, which needs the leap
/// table. Hiding that conversion inside this module would make a leap-second
/// dependency invisible to a caller who has one anyway — L4 integrates in TT and
/// already holds the table. So the crossing is the caller's, made once and
/// explicitly, which is the same reasoning `SPEC-frames` FRAME-R-062 applies to
/// the km/metre crossing.
///
/// Local solar time is NOT a parameter: the reference's own header warns that
/// UT, longitude and local time enter independently and should be consistent,
/// and a three-argument interface invites the inconsistent call and cannot
/// detect it (`ATMO-R-010`). It is derived from `seconds_of_day` and longitude.
struct Place {
    int day_of_year = 1;              ///< 1 … 366 (`ATMO-F-002`)
    double seconds_of_day = 0.0;      ///< UT
    double geodetic_latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_km = 0.0;
};

// --------------------------------------------------------------------------
// Evaluation

/// Densities excluding anomalous oxygen.
[[nodiscard]] odl::Result<NeutralDensity, AtmoError>
neutral(const Place& where, const SpaceWeather& sw);

/// Densities including anomalous oxygen — the drag quantity.
[[nodiscard]] odl::Result<DragDensity, AtmoError>
for_drag(const Place& where, const SpaceWeather& sw);

/// The independent-variable form the reference also offers, where UT, longitude
/// and local solar time are supplied separately.  Named so that using it is a
/// decision: `PICONE02`'s own sensitivity studies are the use case, and a
/// physically consistent evaluation is what `neutral` and `for_drag` give.
[[nodiscard]] odl::Result<DragDensity, AtmoError>
for_drag_with_independent_local_time(const Place& where, const SpaceWeather& sw,
                                     double local_solar_time_hours);

/// The altitude below which `MSIS-FOR`'s header instructs that F10.7, F10.7A and
/// Ap be set to 150, 150 and 4 — *"neither large nor well established below
/// 80 km"* (`ATMO-R-013`).
inline constexpr double kSubstitutionAltitudeKm = 80.0;

/// Two results are comparable only if they came from the same pinned snapshot.
/// Revision means a snapshot can differ from another AT AN EPOCH BOTH COVER, so
/// this is a refusal rather than a caveat (ATMO-R-024, ATMO-F-013).
[[nodiscard]] odl::Result<double, AtmoError>
density_ratio(const DragDensity& a, const DragDensity& b);

}  // namespace odl::atmosphere
