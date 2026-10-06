// SPEC-measmod.md §4.6 — the ephemeris-position model.

#include <odl/measmod/position.hpp>

#include <odl/core/units.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace odl::measmod {

using odl::time::Epoch;

namespace {

const char* system_name(odl::io::Sp3TimeSystem s) {
    switch (s) {
        case odl::io::Sp3TimeSystem::GPS: return "GPS";
        case odl::io::Sp3TimeSystem::GLO: return "GLO";
        case odl::io::Sp3TimeSystem::GAL: return "GAL";
        case odl::io::Sp3TimeSystem::BDT: return "BDT";
        case odl::io::Sp3TimeSystem::TAI: return "TAI";
        case odl::io::Sp3TimeSystem::QZS: return "QZS";
        case odl::io::Sp3TimeSystem::UTC: return "UTC";
    }
    return "?";
}

const char* point_name(ReferencePoint p) {
    switch (p) {
        case ReferencePoint::CentreOfMass: return "CentreOfMass";
        case ReferencePoint::AntennaPhaseCentre: return "AntennaPhaseCentre";
        case ReferencePoint::Unspecified: return "Unspecified";
    }
    return "?";
}

std::string trim_blanks(const std::string& s) {
    const auto first = s.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(' ');
    return s.substr(first, last - first + 1);
}

std::string list_codes() {
    std::string out;
    for (const auto& c : accepted_sp3_frame_codes()) out += (out.empty() ? "" : ", ") + c;
    return out;
}

/// MEAS-R-042: the reference point the model accepts.
odl::Result<void, MeasError> check_point(ReferencePoint p, const char* what) {
    if (p == ReferencePoint::CentreOfMass) return {};
    std::ostringstream m;
    m << what << ": the reference point is " << point_name(p) << ", and only CentreOfMass is modelled — an antenna offset is not (SP3D is silent on which point a position refers to, so the caller says; "
         "accepted: CentreOfMass)";
    return odl::err("MEAS-F-014", m.str());
}

odl::Result<void, MeasError> check_finite(const odl::Vec3& km, const char* what) {
    if (!std::isfinite(km.x) || !std::isfinite(km.y) || !std::isfinite(km.z)) {
        std::ostringstream m;
        m << what << ": a position component is not finite (" << km.x << ", " << km.y << ", " << km.z << " km)";
        return odl::err("MEAS-F-018", m.str());
    }
    return {};
}

}  // namespace

const std::vector<std::string>& accepted_sp3_frame_codes() {
    // MEAS-R-041: the realisations of the ITRS, and the ILRS products' own code for SLRF2020
    static const std::vector<std::string> codes = {"IGS05", "IGS08", "IGb08", "IGS14", "IGb14", "IGS20", "IGb20", "SLR20"};
    return codes;
}

odl::Result<PositionObservation, MeasError> position_observation(const odl::io::Sp3Header& header, const odl::time::Calendar& epoch,
                                                                 const odl::io::Sp3PositionRecord& record, ReferencePoint point,
                                                                 const odl::time::LeapTable& leaps) {
    const char* what = "SP3 position observation";
    // the time system first: a GLONASS, Galileo, BeiDou or QZSS system time has its own epoch or a time-varying offset from GPS time that no TimeScale names
    auto scale = odl::io::to_time_scale(header.time_system);
    if (!scale) {
        std::ostringstream m;
        m << what << ": the header's time system " << system_name(header.time_system) << " is not one of GPS, TAI, UTC (" << scale.error().id << ": " << scale.error().message
          << "); treating it as GPS time would put the epoch up to seconds away";
        return odl::err("MEAS-F-014", m.str());
    }
    // the coordinate-system code, by an explicit table (MEAS-R-041): an unknown code is a different frame, and ITRS is not assumed
    const std::string code = trim_blanks(header.coordinate_sys);
    const auto& codes = accepted_sp3_frame_codes();
    if (std::find(codes.begin(), codes.end(), code) == codes.end()) {
        std::ostringstream m;
        m << what << ": the coordinate-system code '" << code << "' is not in the table of the ITRS realisations (" << list_codes() << ") — treating it as ITRS would hide a different frame";
        return odl::err("MEAS-F-014", m.str());
    }
    if (auto p = check_point(point, what); !p) return odl::err(p.error());
    const odl::Vec3 km{record.x_km, record.y_km, record.z_km};
    if (auto f = check_finite(km, what); !f) return odl::err(f.error());
    if (record.x_km == 0.0 && record.y_km == 0.0 && record.z_km == 0.0) {
        std::ostringstream m;
        m << what << ": the position of satellite " << record.satellite_id << " at " << epoch.year << "-" << epoch.month << "-" << epoch.day << " " << epoch.hour << ":" << epoch.minute << ":" << epoch.second
          << " is exactly (0, 0, 0): SP3D sets a bad or absent position to 0.000000, and a satellite at the Earth's centre is not an observation";
        return odl::err("MEAS-F-018", m.str());
    }
    auto when = Epoch::from_calendar(*scale, epoch, leaps);
    if (!when) return odl::err(with_context(when.error(), "SP3 position observation, the epoch"));
    return PositionObservation{*when, PositionFrame::ITRS, odl::metres_from_km(km), point};
}

odl::Result<PositionObservation, MeasError> position_observation(const odl::io::HorizonsStateRecord& record, ReferencePoint point, const odl::time::LeapTable& leaps) {
    const char* what = "Horizons position observation";
    auto scale = odl::io::to_time_scale(record.time_system);
    if (!scale) return odl::err(with_context(scale.error(), what));                       // the reader's own id (IOHZ-F-002), unchanged
    if (auto p = check_point(point, what); !p) return odl::err(p.error());
    if (auto f = check_finite(record.position_km, what); !f) return odl::err(f.error());
    auto when = Epoch::from_calendar(*scale, record.epoch, leaps);
    if (!when) return odl::err(with_context(when.error(), "Horizons position observation, the epoch"));
    return PositionObservation{*when, PositionFrame::GCRS, odl::metres_from_km(record.position_km), point};
}

odl::Result<ModelledPosition, MeasError> model_position(const PositionObservation& obs, const Trajectory& target, const EarthOrientation& earth) {
    if (!std::isfinite(obs.position_m.x) || !std::isfinite(obs.position_m.y) || !std::isfinite(obs.position_m.z))
        return odl::err("MEAS-F-018", "position model: the observed position is not finite — no residual of NaN is returned");
    if (auto p = check_point(obs.point, "position model"); !p) return odl::err(p.error());
    auto state = target.state_at(obs.epoch);
    if (!state) return odl::err(with_context(state.error(), "position model, the target at the observation epoch"));
    const odl::Vec3 r_gcrs = odl::metres_from_km(state->position());

    PositionApplied applied;
    applied.frame = obs.frame;
    if (obs.frame == PositionFrame::ITRS) {
        auto orientation = earth.at(obs.epoch);
        if (!orientation) return odl::err(with_context(orientation.error(), "position model, the Earth's orientation at the observation epoch"));
        applied.rotation = orientation->gcrs_to_itrs;
        applied.omitted.push_back(OmittedTerm{"itrs_realisation", "millimetres to centimetres between IGS05 … IGb20 and SLR20",
                                              "quoted from memory, not quantified; the SP3 frame code is mapped to one ITRS (MEAS-R-041, SPEC-measmod §6.4)"});
    }
    const odl::Vec3 modelled = applied.rotation.apply(r_gcrs);

    Partials<odl::frames::Frame::GCRS, 3> partials;
    for (std::size_t i = 0; i < 3; ++i) partials.d[i] = {applied.rotation.r[i][0], applied.rotation.r[i][1], applied.rotation.r[i][2], 0.0, 0.0, 0.0};
    return ModelledPosition(modelled, obs.position_m, obs.epoch, partials, std::move(applied));
}

}  // namespace odl::measmod
