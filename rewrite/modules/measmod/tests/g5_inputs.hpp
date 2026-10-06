#pragma once
// g5_inputs.hpp — the inputs of G5, the registered check of the range model against one real pass (SPEC-measmod.md §8.9, MEAS-A-100 and -101).
//
// TEST-SIDE ONLY: nothing in include/ or src/ reads this file. Every reader asserts the facts the specification REGISTERED before applying them (the DHF record, the centre-of-mass
// row, the eccentricity determinations, the products' headers), so a file that changed is a failure and not a different envelope. The pass is chosen by the rule of §8.9 from
// the sessions' metadata alone, and the choice is asserted equal to the one `tools/measmod_g5_select.py` made (output kept as round-9 evidence).
//
// No residual is formed anywhere in this file or in g5_envelope.hpp: the model's answer is read for its GEOMETRY (the Applied record) and its modelled range only;
// `residual_m()` and `observed_range_m()` are never called before the comparison (MEAS-A-100) exists.

#include "solid_tide.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <odl/io/sp3_ephemeris.hpp>
#include <odl/measmod/shapiro.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace odl::measmod::testing::g5 {

using odl::Vec3;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::TimeScale;

// ---- the facts registered in SPEC-measmod §8.9 before they were applied -----------------------------------------------------------------------------------------------------

inline constexpr int kPad = 7090;                                   // Yarragadee (the manager's ruling, e3d7537)
inline constexpr const char* kSodText = "70900513";                 // the one system-occupation number of the six determinations
inline constexpr const char* kChosenStart = "2026-01-02 04:04:45";  // what the rule chose when it was applied (56e0053)
inline constexpr int kChosenNormalPoints = 18;
inline constexpr double kDhfBias_mm = 4.8, kDhfSigma_mm = 3.8;      // the DHF record 7090 51 501 A 22:292:00000 00:000:00000 R 4.8 3.8 mm
inline constexpr double kChosenCom_mm = 246.2;                      // com_lg1.dat, 7090, 11 09 2017 .. 01 01 2050, 532 nm
inline constexpr double kEccScatter_mm[3] = {1.8, 3.4, 3.0};        // up / north / east, maximum - minimum of the six determinations
inline constexpr double kSlrfReferenceEpochYear = 2015.0;           // REFEPOCH 15:001:00000 of the SLRF2020 solution for pad 7090

// ---- small text helpers --------------------------------------------------------------------------------------------------------------------------------------------------------

inline std::vector<std::string> split_ws(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    for (std::string tok; in >> tok;) out.push_back(tok);
    return out;
}

inline std::vector<std::string> text_lines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string l; std::getline(in, l);) out.push_back(l);
    return out;
}

inline int day_of_year(int y, int m, int d) {
    using namespace std::chrono;
    const sys_days day = sys_days{year{y} / month{static_cast<unsigned>(m)} / std::chrono::day{static_cast<unsigned>(d)}};
    const sys_days j1 = sys_days{year{y} / January / 1};
    return static_cast<int>((day - j1).count()) + 1;
}

inline Calendar utc_calendar_of(const Epoch& e, int decimals = 9) {
    auto c = e.calendar(TimeScale::UTC, leaps(), decimals);
    REQUIRE(c.has_value());
    return *c;
}

// ---- the Data Handling File: every record of the pad that covers the epoch --------------------------------------------------------------------------------------------------

struct DhfRecord {
    std::string line;
    std::string target;        ///< token 2: "51" LAGEOS-1, "52" LAGEOS-2, "--" any
    std::string type;          ///< "R" range bias, "E", "X" excluded, "T" time bias ...
    double value = 0.0, sigma = 0.0;
};

/// A SINEX epoch `YY:DDD:SSSSS` as a sortable key (years 00-49 are 20YY, 50-99 are 19YY, as the registry reads the same files); `00:000:00000` is "no epoch" (`unset`). The DHF's own blocks write the end of a
/// day as `86400` (the time-bias block), which the registry's decoder, written for the SLRF files' `86399`, refuses: so this test-side reader decodes for itself.
struct SinexKey {
    bool unset = false;
    long key = 0;
};

inline SinexKey sinex_key_of(const std::string& text) {
    const int yy = std::stoi(text.substr(0, 2)), doy = std::stoi(text.substr(3, 3)), sod = std::stoi(text.substr(7, 5));
    SinexKey k;
    k.unset = (yy == 0 && doy == 0 && sod == 0);
    const int year = yy < 50 ? 2000 + yy : 1900 + yy;
    k.key = static_cast<long>(year) * 100000000L + static_cast<long>(doy) * 100000L + sod;
    return k;
}

/// Every record of the DHF — any of its blocks — that names `pad` and whose span `[start, end]` contains `when` (UTC). `00:000:00000` is open. A record is recognised by the two
/// SINEX epochs in tokens 4 and 5, which the three blocks that carry spans share.
inline std::vector<DhfRecord> dhf_records_covering(int pad, const Calendar& when) {
    const std::regex sinex_time(R"(^\d\d:\d\d\d:\d\d\d\d\d$)");
    const long when_key = static_cast<long>(when.year) * 100000000L + static_cast<long>(day_of_year(when.year, when.month, when.day)) * 100000L +
                          static_cast<long>(when.hour * 3600 + when.minute * 60 + static_cast<int>(when.second));
    std::vector<DhfRecord> out;
    for (const std::string& line : text_lines(slurp(ODL_DHF_FILE))) {
        const auto tok = split_ws(line);
        if (tok.size() < 7 || tok[0] != std::to_string(pad)) continue;
        if (!std::regex_match(tok[4], sinex_time) || !std::regex_match(tok[5], sinex_time)) continue;
        const SinexKey s = sinex_key_of(tok[4]), e = sinex_key_of(tok[5]);
        const bool after_start = s.unset || s.key <= when_key;
        const bool before_end = e.unset || when_key <= e.key;
        if (!(after_start && before_end)) continue;
        DhfRecord r{line, tok[1], tok[6], 0.0, 0.0};
        if (r.type == "R" && tok.size() >= 9) {
            r.value = std::stod(tok[7]);
            r.sigma = std::stod(tok[8]);
        }
        out.push_back(r);
    }
    return out;
}

// ---- the centre-of-mass tables ----------------------------------------------------------------------------------------------------------------------------------------------

struct ComRow {
    int station = 0;
    int start_d = 0, start_m = 0, start_y = 0, end_d = 0, end_m = 0, end_y = 0;
    int wavelength_nm = 0;
    double mm = 0.0;
};

inline const std::vector<ComRow>& com_rows() {
    static const std::vector<ComRow> rows = [] {
        std::vector<ComRow> out;
        for (const std::string& line : text_lines(slurp(ODL_COM_LG1_FILE))) {
            if (line.empty() || line[0] == '*') continue;
            const auto t = split_ws(line);
            if (t.size() != 9) continue;
            ComRow r;
            r.station = std::stoi(t[0]);
            r.start_d = std::stoi(t[1]); r.start_m = std::stoi(t[2]); r.start_y = std::stoi(t[3]);
            r.end_d = std::stoi(t[4]); r.end_m = std::stoi(t[5]); r.end_y = std::stoi(t[6]);
            r.wavelength_nm = std::stoi(t[7]);
            r.mm = std::stod(t[8]);
            out.push_back(r);
        }
        return out;
    }();
    return rows;
}

inline bool com_covers(const ComRow& r, int y, int m, int d) {
    const long k = y * 10000L + m * 100L + d;
    return r.start_y * 10000L + r.start_m * 100L + r.start_d <= k && k < r.end_y * 10000L + r.end_m * 100L + r.end_d;
}

/// The rows of `station` at `wavelength_nm` whose span contains the date. The tables' readme: when more than one covers an epoch, the last is the most often used system.
inline std::vector<ComRow> com_rows_covering(int station, int wavelength_nm, int y, int m, int d) {
    std::vector<ComRow> out;
    for (const ComRow& r : com_rows())
        if (r.station == station && r.wavelength_nm == wavelength_nm && com_covers(r, y, m, d)) out.push_back(r);
    return out;
}

// ---- the eccentricity file's active determinations of one SOD --------------------------------------------------------------------------------------------------------------------

struct EccDetermination {
    double up = 0.0, north = 0.0, east = 0.0;
    std::string line;
};

/// The non-comment `UNE` records of the system-occupation number (the file keeps superseded values as `*` comment lines, which are not read).
inline std::vector<EccDetermination> ecc_determinations(const std::string& sod_text) {
    std::vector<EccDetermination> out;
    for (const std::string& line : text_lines(slurp(ODL_SLR_ECC_FILE))) {
        if (line.empty() || line[0] == '*') continue;
        const auto t = split_ws(line);
        if (t.size() != 11 || t[6] != "UNE" || t[10] != sod_text || t[0] != std::to_string(kPad)) continue;
        out.push_back(EccDetermination{std::stod(t[7]), std::stod(t[8]), std::stod(t[9]), line});
    }
    return out;
}

// ---- the C04 file's own standard deviations ------------------------------------------------------------------------------------------------------------------------------

struct C04Sigmas {
    double ut1_s = 0.0;           ///< UT1-UTC Er
    double x_arcsec = 0.0, y_arcsec = 0.0;
};

inline C04Sigmas c04_sigmas_on(int y, int m, int d) {
    for (const std::string& line : text_lines(slurp(ODL_C04_FILE))) {
        if (line.empty() || line[0] == '#') continue;
        const auto t = split_ws(line);
        if (t.size() < 20) continue;
        if (std::stoi(t[0]) == y && std::stoi(t[1]) == m && std::stoi(t[2]) == d) return C04Sigmas{std::stod(t[15]), std::stod(t[13]), std::stod(t[14])};
    }
    FAIL("the C04 file has no row for " << y << "-" << m << "-" << d);
    return {};
}

// ---- the nine other products of the week, and the primary --------------------------------------------------------------------------------------------------------------------

struct Sp3Product {
    std::string ac;                     ///< "ilrsa" (the primary the model uses), "ilrsb", "asi", ...
    std::string coordinate_sys, agency;
    odl::io::Sp3TimeSystem time_system;
    odl::io::Sp3Ephemeris eph;
    Epoch first;                        ///< the first sample's epoch (UTC)
};

inline const std::vector<Sp3Product>& sp3_products() {
    static const std::vector<Sp3Product> products = [] {
        struct Src { const char* ac; const char* path; };
        const Src sources[] = {{"ilrsa", ODL_ILRS_SP3_FILE}, {"ilrsb", ODL_SP3_ILRSB_FILE}, {"asi", ODL_SP3_ASI_FILE}, {"bkg", ODL_SP3_BKG_FILE}, {"cnes", ODL_SP3_CNES_FILE},
                               {"dgfi", ODL_SP3_DGFI_FILE}, {"esa", ODL_SP3_ESA_FILE}, {"gfz", ODL_SP3_GFZ_FILE}, {"jcet", ODL_SP3_JCET_FILE}, {"nsgf", ODL_SP3_NSGF_FILE}};
        std::vector<Sp3Product> out;
        for (const Src& s : sources) {
            auto file = odl::io::read_sp3(slurp(s.path));
            if (!file) FAIL(s.ac << ": the SP3 did not read: " << file.error().id << " " << file.error().message);
            auto eph = odl::io::Sp3Ephemeris::build(*file, "L51");
            if (!eph) FAIL(s.ac << ": no ephemeris of L51: " << eph.error().id << " " << eph.error().message);
            auto first = Epoch::from_calendar(TimeScale::UTC, eph->first_epoch(), leaps());
            if (!first) FAIL(s.ac << ": the first epoch did not convert");
            out.push_back(Sp3Product{s.ac, file->header.coordinate_sys, file->header.agency, file->header.time_system, std::move(*eph), *first});
        }
        return out;
    }();
    return products;
}

/// The product's position of L51 at `when`, ITRS (the product's own coordinate system), metres, by the one SP3 interpolation facility (11 points). The product's time system is UTC
/// (asserted by the callers) and its week has no leap second, so the elapsed SI seconds from its first epoch are the elapsed calendar seconds the facility wants.
inline odl::Result<Vec3, MeasError> sp3_position_itrs_m(const Sp3Product& p, const Epoch& when) {
    auto r = p.eph.position_km_at(when.difference(p.first).to_seconds());
    if (!r) return odl::err(r.error().id, r.error().message);
    return odl::metres_from_km(*r);
}

// ---- the orientation, the target, the station: the model's inputs -----------------------------------------------------------------------------------------------------

/// The real chain's Earth orientation (C04, the default policy), the one the model is given.
inline const EopEarthOrientation& orientation() {
    static const EopEarthOrientation o(c04(), odl::eop::EopPolicy{}, leaps());
    return o;
}

/// The primary product as the model's target: the ITRS position at the epoch rotated to the GCRS by the real chain; the velocity is the central difference of the TRANSFORMED positions
/// at ±1 s (the io header's own instruction: a difference of the rotated positions, not a rotation of the difference).
class Sp3Trajectory final : public Trajectory {
public:
    Sp3Trajectory(const Sp3Product& product, const EarthOrientation& earth) : product_(&product), earth_(&earth) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const Epoch& when) const override {
        auto r0 = position_gcrs_m(when);
        if (!r0) return odl::err(r0.error());
        auto rp = position_gcrs_m(when.add(odl::time::Duration::from_seconds(1.0)));
        if (!rp) return odl::err(rp.error());
        auto rm = position_gcrs_m(when.add(odl::time::Duration::from_seconds(-1.0)));
        if (!rm) return odl::err(rm.error());
        const Vec3 v = 0.5 * (*rp - *rm);
        return odl::frames::GcrsState{when, odl::km_from_metres(*r0), odl::km_from_metres(v)};
    }
    [[nodiscard]] odl::Result<Vec3, MeasError> position_gcrs_m(const Epoch& when) const {
        auto itrs = sp3_position_itrs_m(*product_, when);
        if (!itrs) return odl::err(itrs.error());
        auto o = earth_->at(when);
        if (!o) return odl::err(o.error());
        return o->gcrs_to_itrs.transpose().apply(*itrs);
    }

private:
    const Sp3Product* product_;
    const EarthOrientation* earth_;
};

inline const Sp3Trajectory& target() {
    static const Sp3Trajectory t(sp3_products().front(), orientation());
    return t;
}

/// A station that does not turn with the Earth: its kinematics at one epoch held for every epoch, velocity and vertical rate zero (the defect "the Earth's rotation during the light
/// time omitted", as MEAS-A-032 shows it).
class FrozenStation final : public StationTrack {
public:
    explicit FrozenStation(StationKinematics k) : k_(k) {
        k_.velocity_m_s = Vec3{};
        k_.up_rate_per_s = Vec3{};
    }
    [[nodiscard]] odl::Result<StationKinematics, MeasError> at(const Epoch&) const override { return k_; }

private:
    StationKinematics k_;
};

// ---- the pass, chosen by the registered rule from the sessions' metadata -------------------------------------------------------------------------------------------------

struct PassChoice {
    std::size_t index = 0;                          ///< into real_passes()
    int normal_points = 0;
    std::string start_text;                         ///< "YYYY-MM-DD HH:MM:SS"
    std::vector<std::string> ranking;               ///< the rule's order, printed
    std::vector<std::string> skipped;               ///< sessions the builder refused for their own metadata (clause of the rule), with the reason
    std::vector<RangeObservation> observations;     ///< the normal points of the chosen pass, in file order
    std::vector<const odl::io::CrdRangeRecord*> records;   ///< the record `11`s, parallel to `observations`
    Calendar start_calendar;
};

inline SphericalCentreOfMass com_for(const std::vector<ComRow>& rows) {
    REQUIRE(rows.size() >= 1);
    auto c = SphericalCentreOfMass::make(kLageos1Id, rows.back().mm * 1e-3,
                                         "Rodriguez, Otsubo & Appleby 2019, doi:10.1007/s00190-019-01315-0; com_lg1.dat of the ILRS tables generated 2026-01-12 (pinned ilrs-com6-260112)");
    REQUIRE(c.has_value());
    return *c;
}

inline const PassChoice& chosen() {
    static const PassChoice choice = [] {
        const auto& passes = real_passes();
        const Epoch window_start = utc(2026, 1, 1, 0, 0, 0.0), window_end = utc(2026, 1, 3, 23, 0, 0.0);
        struct Candidate {
            std::size_t index;
            int nps;
            Epoch start;
            std::string text;
        };
        std::vector<Candidate> eligible;
        for (std::size_t i = 0; i < passes.size(); ++i) {
            const odl::io::CrdPass& p = passes[i];
            if (p.station.system_id != kPad) continue;
            const auto& s = p.session;
            if (!(s.end_year && s.end_month && s.end_day && s.end_hour && s.end_minute && s.end_second)) continue;
            const Epoch start = utc(s.start_year, s.start_month, s.start_day, s.start_hour, s.start_minute, s.start_second);
            const Epoch end = utc(*s.end_year, *s.end_month, *s.end_day, *s.end_hour, *s.end_minute, *s.end_second);
            if (!(window_start <= start && end <= window_end)) continue;
            REQUIRE_FALSE(s.remaining_fields.empty());
            if (std::stoi(s.remaining_fields.back()) != 0) continue;           // the H4 data-quality alert indicator
            const int nps = static_cast<int>(std::count_if(p.ranges.begin(), p.ranges.end(), [](const odl::io::CrdRangeRecord& r) { return r.kind == odl::io::CrdRecordKind::NormalPointRange; }));
            char buf[32];
            std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d", s.start_year, s.start_month, s.start_day, s.start_hour, s.start_minute, s.start_second);
            eligible.push_back(Candidate{i, nps, start, buf});
        }
        std::sort(eligible.begin(), eligible.end(), [](const Candidate& a, const Candidate& b) { return a.nps != b.nps ? a.nps > b.nps : a.start < b.start; });
        REQUIRE_FALSE(eligible.empty());

        PassChoice out;
        for (const Candidate& c : eligible) {
            const odl::io::CrdPass& p = passes[c.index];
            out.ranking.push_back(c.text + "  " + std::to_string(c.nps) + " normal points");
            const auto& s = p.session;
            const auto com_rows = com_rows_covering(kPad, 532, s.start_year, s.start_month, s.start_day);
            auto obs = range_observations(p, real_registry(), leaps(), com_for(com_rows));
            if (!obs) {
                out.skipped.push_back(c.text + ": " + std::string(obs.error().id) + " " + obs.error().message);
                continue;
            }
            out.index = c.index;
            out.normal_points = c.nps;
            out.start_text = c.text;
            out.observations = std::move(*obs);
            for (const odl::io::CrdRangeRecord& r : p.ranges)
                if (r.kind == odl::io::CrdRecordKind::NormalPointRange) out.records.push_back(&r);
            out.start_calendar = Calendar{s.start_year, s.start_month, s.start_day, s.start_hour, s.start_minute, static_cast<double>(s.start_second)};
            return out;
        }
        FAIL("no eligible session was accepted by the observation builder");
        return out;
    }();
    return choice;
}

}  // namespace odl::measmod::testing::g5
