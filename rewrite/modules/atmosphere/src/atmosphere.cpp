// atmosphere.cpp — the module boundary: units, refusals, provenance.
//
// SPEC-atmosphere.md.  Everything above this file is the reference's own
// arithmetic in the reference's own CGS units; everything below it is SI with
// provenance attached.  The crossing happens ONCE, here (ATMO-R-006), the way
// SPEC-frames FRAME-R-062 requires of the km/metre crossing.

#include <odl/atmosphere/atmosphere.hpp>

#include "msis_model.hpp"

#include <cmath>
#include <string>

namespace odl::atmosphere {
namespace {

using detail::Fault;

constexpr double kCm3ToM3 = 1.0e6;        ///< cm^-3 -> m^-3
constexpr double kGcm3ToKgm3 = 1.0e3;     ///< g cm^-3 -> kg m^-3
constexpr double kAmuGrams = 1.66e-24;    ///< the reference's own value

/// The species masses, in the reference's index order.
constexpr double kMass[9] = {4.0, 16.0, 28.0, 32.0, 40.0, 0.0, 1.0, 14.0, 16.0};

/// ATMO-R-036: the materiality threshold is a DRAG threshold.  Below it the
/// model does not resolve a species, and its number density is a refusal rather
/// than a small number.
constexpr double kMaterialFraction = 1.0e-15;

AtmoError fault_diagnostic(const detail::FaultRecord& f) {
    switch (f.what) {
    case Fault::coincident_spline_node:
        return {"ATMO-F-012",
                "two spline nodes coincide at " + std::to_string(f.a) + " km, so the "
                "temperature profile is not interpolable there (requested " +
                std::to_string(f.c) + " km). The NRL reference prints 'BAD XA INPUT TO SPLINT' "
                "and divides by zero anyway; this module refuses."};
    case Fault::nonpositive_density_ratio:
        return {"ATMO-F-012",
                "a non-positive density reached the turbopause blend before its logarithm: "
                "mixed " + std::to_string(f.a) + ", diffusive " + std::to_string(f.b) +
                ", species mass " + std::to_string(f.c) +
                ". The NRL reference prints 'DNET LOG ERROR', patches a zero to 1.0 and "
                "continues; this module refuses."};
    case Fault::pressure_no_convergence:
        return {"ATMO-F-012", "the pressure-level iteration did not converge"};
    case Fault::none: break;
    }
    return {"ATMO-F-012", "no fault"};
}

Species to_species(const std::array<double, 9>& d, double rho_gcm3) {
    Species s;
    SpeciesDensity* out[9] = {&s.he, &s.o, &s.n2, &s.o2, &s.ar,
                              nullptr, &s.h, &s.n, &s.anomalous_o};
    for (std::size_t j = 0; j < 9; ++j) {
        if (!out[j]) continue;
        const double mass_gcm3 = kMass[j] * d[j] * kAmuGrams;
        const double frac = (rho_gcm3 > 0.0) ? mass_gcm3 / rho_gcm3 : 0.0;
        *out[j] = SpeciesDensity{d[j] * kCm3ToM3, mass_gcm3 * kGcm3ToKgm3,
                                 frac >= kMaterialFraction};
    }
    return s;
}

EvaluationRecord record_of(const SpaceWeather& sw, bool substituted) {
    EvaluationRecord r;
    r.snapshot_id = sw.snapshot_id;
    r.snapshot_sha256 = sw.snapshot_sha256;
    r.data_class = sw.data_class;
    r.verification = sw.verification;
    r.sub_80km_substitution = substituted;
    r.year_discarded = true;
    return r;
}

}  // namespace

odl::Result<double, AtmoError> SpeciesDensity::number_density_m3() const {
    if (!resolved_) {
        return odl::err(AtmoError{
            "ATMO-F-017",
            "this species is below what NRLMSISE-00 resolves at this state: its mass "
            "contributes less than 1e-15 of the total density, where the reference's own "
            "single-precision arithmetic loses significance and then underflows to a hard "
            "zero. That is the right answer for drag and not for a number-density consumer, "
            "so it is refused rather than returned. Use mass_density_kg_m3() if the drag "
            "contribution is what you need."});
    }
    return number_m3_;
}

odl::Result<double, AtmoError>
density_ratio(const DragDensity& a, const DragDensity& b) {
    if (a.record.snapshot_sha256 != b.record.snapshot_sha256) {
        return odl::err(AtmoError{
            "ATMO-F-013",
            "these two results came from different space-weather snapshots (" +
            a.record.snapshot_id + " sha256 " + a.record.snapshot_sha256.substr(0, 16) +
            "... and " + b.record.snapshot_id + " sha256 " +
            b.record.snapshot_sha256.substr(0, 16) +
            "...), and the issuing authority REVISES rows in place, so they may differ at an "
            "epoch both cover. They are not comparable."});
    }
    if (b.total_mass_kg_m3 == 0.0) {
        return odl::err(AtmoError{"ATMO-F-013", "the denominator density is zero"});
    }
    return a.total_mass_kg_m3 / b.total_mass_kg_m3;
}

}  // namespace odl::atmosphere

// --------------------------------------------------------------------------
// Evaluation.  This is the whole of the module boundary: refuse what §4.2 says
// to refuse, cross into SI once, and attach the provenance.

namespace odl::atmosphere {
namespace {

odl::Result<detail::RawResult, AtmoError>
evaluate(const Place& p, const SpaceWeather& sw, double stl, bool drag, bool& substituted) {
    if (p.day_of_year < 1 || p.day_of_year > 366) {
        return odl::err(AtmoError{"ATMO-F-002",
            "day of year " + std::to_string(p.day_of_year) + " is outside 1 … 366, which is the "
            "range MSIS-FOR's own header states."});
    }
    if (p.altitude_km < 0.0) {
        return odl::err(AtmoError{"ATMO-F-001",
            "altitude " + std::to_string(p.altitude_km) + " km is below 0 km. The model's "
            "lowest spline node IS 0 km (ZN3(5)), so below it the fitted cubic is being "
            "evaluated outside its own knots. The NRL reference has no altitude check at all "
            "and would return a number; this module refuses (ATMO-R-015)."});
    }
    double f107 = sw.f107_previous_day;
    double f107a = sw.f107a_centred81;
    DailyAp daily = sw.daily;
    ThreeHourlyAp hourly = sw.three_hourly;
    substituted = false;
    if (p.altitude_km < kSubstitutionAltitudeKm) {
        // MSIS-FOR's header: below 80 km these effects are "neither large nor
        // well established" and the parameters "should be set to 150., 150. and
        // 4." It is the source's own instruction, and it is recorded, not silent.
        substituted = (f107 != 150.0 || f107a != 150.0 || daily.ap != 4.0);
        f107 = 150.0; f107a = 150.0; daily.ap = 4.0; hourly.ap.fill(4.0);
    } else {
        if (!(f107 > 0.0) || !(f107a > 0.0)) {
            return odl::err(AtmoError{"ATMO-F-006",
                "F10.7 must be positive; got " + std::to_string(f107) + " and " +
                std::to_string(f107a) + ". The file's -1 is an absence marker, not a flux."});
        }
        if (daily.ap < 0.0) {
            return odl::err(AtmoError{"ATMO-F-006",
                "Ap must not be negative; got " + std::to_string(daily.ap) + "."});
        }
    }
    const bool three = sw.has_three_hourly && p.altitude_km >= kSubstitutionAltitudeKm;
    auto r = drag ? detail::gtd7d(p.day_of_year, p.seconds_of_day, p.altitude_km,
                                  p.geodetic_latitude_deg, p.longitude_deg, stl,
                                  f107a, f107, daily, hourly, three)
                  : detail::gtd7(p.day_of_year, p.seconds_of_day, p.altitude_km,
                                 p.geodetic_latitude_deg, p.longitude_deg, stl,
                                 f107a, f107, daily, hourly, three);
    if (!r.fault.ok()) return odl::err(fault_diagnostic(r.fault));
    return r;
}

double derived_local_time(const Place& p) noexcept {
    return p.seconds_of_day / 3600.0 + p.longitude_deg / 15.0;
}

}  // namespace

odl::Result<NeutralDensity, AtmoError> neutral(const Place& p, const SpaceWeather& sw) {
    bool sub = false;
    auto r = evaluate(p, sw, derived_local_time(p), false, sub);
    if (!r) return odl::err(r.error());
    NeutralDensity out;
    out.species = to_species(r->d, r->d[5]);
    out.temperature = {r->t[0], r->t[1]};
    out.total_mass_kg_m3 = r->d[5] * kGcm3ToKgm3;
    out.record = record_of(sw, sub);
    return out;
}

odl::Result<DragDensity, AtmoError> for_drag(const Place& p, const SpaceWeather& sw) {
    bool sub = false;
    auto r = evaluate(p, sw, derived_local_time(p), true, sub);
    if (!r) return odl::err(r.error());
    DragDensity out;
    out.species = to_species(r->d, r->d[5]);
    out.temperature = {r->t[0], r->t[1]};
    out.total_mass_kg_m3 = r->d[5] * kGcm3ToKgm3;
    out.record = record_of(sw, sub);
    return out;
}

odl::Result<DragDensity, AtmoError>
for_drag_with_independent_local_time(const Place& p, const SpaceWeather& sw, double stl_hours) {
    bool sub = false;
    auto r = evaluate(p, sw, stl_hours, true, sub);
    if (!r) return odl::err(r.error());
    DragDensity out;
    out.species = to_species(r->d, r->d[5]);
    out.temperature = {r->t[0], r->t[1]};
    out.total_mass_kg_m3 = r->d[5] * kGcm3ToKgm3;
    out.record = record_of(sw, sub);
    return out;
}

}  // namespace odl::atmosphere
