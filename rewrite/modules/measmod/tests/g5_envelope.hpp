#pragma once
// g5_envelope.hpp — the envelope of G5 (SPEC-measmod.md §8.9, MEAS-A-101) and the assembly defects it is measured against.
//
// For normal point i of the chosen pass the envelope is [c_i − w_i, c_i + w_i] on the residual `observed − modelled` of the one-way-equivalent range, no bias removed:
//   c_i = −ĝ_i · Δr_i   the signed range effect of the omitted solid Earth tide (the test-side Step-1 helper, de440s Sun and Moon),
//   w_i = the worst-case linear sum of twelve unsigned terms (rules 1–10 of §8.9; rule 8 is three terms and rule 9 two).
// Nothing here forms a residual: the model's answer is read for its GEOMETRY (ĝ, e, the mapping function, the delays) and its modelled range alone.

#include "g5_inputs.hpp"

#include <numbers>
#include <optional>
#include <utility>

namespace odl::measmod::testing::g5 {

inline constexpr double kC = odl::measmod::kSpeedOfLight_m_s;
inline constexpr double kOrientationPoleCoefficient_m_per_mas = 30.92e-3;     // MEAS-P-33
inline constexpr double kOrientationUt1Coefficient_m_per_us = 0.465e-3;       // MEAS-P-5

// ---- the term functions: the closed forms of §8.9's rules (MEAS-A-101c checks each at a hand-computed value) -----------------------------------------------------------------

inline double deg_of(double rad) { return rad * 180.0 / std::numbers::pi; }
inline double pole_tide_term_m(double e_rad) { return 0.025 * std::sin(e_rad) + 0.007 * std::cos(e_rad); }                // 25 mm radial, 7 mm horizontal, projected and summed
inline double zenith_delay_term_m(double mapping) { return 3.0 * 0.001 * mapping; }                                      // 3 x 1 mm x m(e)
inline double gradient_term_m(double e_rad) {                                                                           // min(50 mm, 2 mm m_g(e)), m_g = 1/(sin e tan e + 0.0031)
    const double mg = 1.0 / (std::sin(e_rad) * std::tan(e_rad) + 0.0031);
    return std::min(0.050, 0.002 * mg);
}
inline double mapping_function_term_m(double e_rad) {                                                                   // 3 x rms(e): 1 / 4 / 16 mm for >= 15 / 10..15 / < 10 degrees
    const double d = deg_of(e_rad);
    return 3.0 * (d >= 15.0 ? 0.001 : d >= 10.0 ? 0.004 : 0.016);
}
inline double orientation_ut1_term_m(double sigma_ut1_s) { return 3.0 * (sigma_ut1_s * 1e6) * kOrientationUt1Coefficient_m_per_us; }
inline double orientation_pole_term_m(double sigma_pole_arcsec) { return 3.0 * (sigma_pole_arcsec * 1e3) * kOrientationPoleCoefficient_m_per_mas; }
inline double normal_point_term_m(double rms_ps, double raw_count) { return 3.0 * 0.5 * kC * (rms_ps * 1e-12) / std::sqrt(raw_count); }   // the RMS is a two-way time
inline double unit_projection_m(const Vec3& ghat, const Vec3& a, const Vec3& b) { return ghat.dot(a - b); }

struct EnvelopeTerms {
    double u_h = 0.0, pole_tide = 0.0, ocean = 0.0, orbit = 0.0, com = 0.0, dhf = 0.0, ecc = 0.0;
    double ztd = 0.0, gradient = 0.0, mapping = 0.0, eop_ut1 = 0.0, eop_pole = 0.0, np_own = 0.0;
    [[nodiscard]] double sum() const { return u_h + pole_tide + ocean + orbit + com + dhf + ecc + ztd + gradient + mapping + eop_ut1 + eop_pole + np_own; }
};

struct NpEnvelope {
    std::size_t index = 0;
    double seconds_of_day = 0.0;
    double elevation_deg = 0.0;
    double range_m = 0.0;                    ///< the up leg's geometric range (geometry, for printing)
    double ghat_dot_station_velocity = 0.0;  ///< ĝ · v_station, m/s (what the omitted-rotation defect is proportional to)
    Vec3 ghat;
    Vec3 tide_gcrs_m;
    double centre_m = 0.0;
    EnvelopeTerms terms;
    double w_m = 0.0;
    double orbit_here_m = 0.0;               ///< this point's own maximum over the products (printed; the term uses the pass's maximum)
    bool beyond_mapping_table = false;       ///< elevation below 6 degrees
};

struct Envelope {
    std::vector<NpEnvelope> points;
    // the inputs read, each asserted against what was registered
    DhfRecord dhf;
    ComRow com_row;
    double com_min_mm = 0.0, com_max_mm = 0.0, com_term_mm = 0.0;
    std::array<double, 3> ecc_scatter_mm{};
    double ecc_norm_mm = 0.0;
    double sigma_ut1_s = 0.0, sigma_pole_arcsec = 0.0;
    C04Sigmas sigmas_day0, sigmas_day1;
    double orbit_max_m = 0.0;
    std::vector<std::pair<std::string, double>> orbit_by_product_m;     ///< per product, the maximum over the pass's points
};

inline void next_day_of(int y, int m, int d, int* ny, int* nm, int* nd) {
    using namespace std::chrono;
    const sys_days next = sys_days{year{y} / month{static_cast<unsigned>(m)} / day{static_cast<unsigned>(d)}} + days{1};
    const year_month_day ymd{next};
    *ny = static_cast<int>(ymd.year());
    *nm = static_cast<int>(static_cast<unsigned>(ymd.month()));
    *nd = static_cast<int>(static_cast<unsigned>(ymd.day()));
}

inline Envelope build_envelope() {
    const PassChoice& pc = chosen();
    const auto& products = sp3_products();
    Envelope env;

    // ---- the inputs, each against what §8.9 registered ----------------------------------------------------------------------------------------------------------------------
    const Calendar day0 = pc.start_calendar;
    {
        const auto recs = dhf_records_covering(kPad, day0);
        std::vector<DhfRecord> mine;
        for (const DhfRecord& r : recs)
            if (r.target == "51" || r.target == "--") mine.push_back(r);
        REQUIRE(mine.size() == 1);
        REQUIRE(mine.front().type == "R");
        env.dhf = mine.front();
        CHECK(env.dhf.value == kDhfBias_mm);
        CHECK(env.dhf.sigma == kDhfSigma_mm);
    }
    {
        const auto rows = com_rows_covering(kPad, 532, day0.year, day0.month, day0.day);
        REQUIRE(rows.size() == 1);
        env.com_row = rows.front();
        CHECK(env.com_row.mm == kChosenCom_mm);
        env.com_min_mm = 1e9;
        env.com_max_mm = -1e9;
        double worst = 0.0;
        for (const ComRow& r : com_rows()) {
            if (r.wavelength_nm != 532) continue;
            env.com_min_mm = std::min(env.com_min_mm, r.mm);
            env.com_max_mm = std::max(env.com_max_mm, r.mm);
            worst = std::max(worst, std::abs(r.mm - env.com_row.mm));
        }
        env.com_term_mm = worst;
    }
    {
        const auto det = ecc_determinations(kSodText);
        REQUIRE(det.size() == 6);
        double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
        for (const EccDetermination& d : det) {
            const double v[3] = {d.up, d.north, d.east};
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], v[k]);
                hi[k] = std::max(hi[k], v[k]);
            }
        }
        double sq = 0.0;
        for (int k = 0; k < 3; ++k) {
            env.ecc_scatter_mm[static_cast<std::size_t>(k)] = (hi[k] - lo[k]) * 1e3;
            CHECK(std::abs(env.ecc_scatter_mm[static_cast<std::size_t>(k)] - kEccScatter_mm[k]) <= 0.05);
            sq += env.ecc_scatter_mm[static_cast<std::size_t>(k)] * env.ecc_scatter_mm[static_cast<std::size_t>(k)];
        }
        env.ecc_norm_mm = std::sqrt(sq);
    }
    {
        int ny = 0, nm = 0, nd = 0;
        next_day_of(day0.year, day0.month, day0.day, &ny, &nm, &nd);
        env.sigmas_day0 = c04_sigmas_on(day0.year, day0.month, day0.day);
        env.sigmas_day1 = c04_sigmas_on(ny, nm, nd);
        env.sigma_ut1_s = std::max(env.sigmas_day0.ut1_s, env.sigmas_day1.ut1_s);
        env.sigma_pole_arcsec = std::max(std::hypot(env.sigmas_day0.x_arcsec, env.sigmas_day0.y_arcsec), std::hypot(env.sigmas_day1.x_arcsec, env.sigmas_day1.y_arcsec));
    }
    for (const Sp3Product& p : products) CHECK(p.time_system == odl::io::Sp3TimeSystem::UTC);

    // ---- the orbit term (rule 4): the maximum over the nine products and the points ------------------------------------------------------------------------------------
    std::vector<double> orbit_here(pc.observations.size(), 0.0);
    env.orbit_by_product_m.reserve(products.size() - 1);
    for (std::size_t x = 1; x < products.size(); ++x) env.orbit_by_product_m.emplace_back(products[x].ac, 0.0);
    for (std::size_t i = 0; i < pc.observations.size(); ++i) {
        const RangeObservation& obs = pc.observations[i];
        auto ra = sp3_position_itrs_m(products.front(), obs.epoch);
        REQUIRE(ra.has_value());
        const Vec3 d = *ra - obs.site.srp_itrs_m;
        const Vec3 ghat_itrs = (1.0 / d.norm()) * d;
        for (std::size_t x = 1; x < products.size(); ++x) {
            auto rx = sp3_position_itrs_m(products[x], obs.epoch);
            REQUIRE(rx.has_value());
            const double spread = std::abs(unit_projection_m(ghat_itrs, *rx, *ra));
            orbit_here[i] = std::max(orbit_here[i], spread);
            env.orbit_by_product_m[x - 1].second = std::max(env.orbit_by_product_m[x - 1].second, spread);
        }
        env.orbit_max_m = std::max(env.orbit_max_m, orbit_here[i]);
    }

    // ---- per normal point ----------------------------------------------------------------------------------------------------------------------------------------------------
    const TideConstants tide;
    for (std::size_t i = 0; i < pc.observations.size(); ++i) {
        const RangeObservation& obs = pc.observations[i];
        const odl::io::CrdRangeRecord& rec = *pc.records[i];
        const EarthFixedStation station(obs.site.srp_itrs_m, obs.site.srp_geodetic, orientation());
        auto m = model_range(obs, station, target());
        REQUIRE(m.has_value());
        const RangeApplied& a = m->applied();           // GEOMETRY only: no residual, no observed range
        NpEnvelope p;
        p.index = i;
        p.seconds_of_day = rec.seconds_of_day;
        const Vec3 gsum = a.up.direction + a.down.direction;
        p.ghat = (1.0 / gsum.norm()) * gsum;
        const double e = 0.5 * (a.up.elevation_rad + a.down.elevation_rad);
        p.elevation_deg = deg_of(e);
        p.range_m = a.up.geometric_range_m;
        p.beyond_mapping_table = p.elevation_deg < 6.0;

        auto sk = station.at(obs.epoch);
        REQUIRE(sk.has_value());
        p.ghat_dot_station_velocity = p.ghat.dot(sk->velocity_m_s);
        auto moon = ephemeris_de440s().geocentric_state(odl::eph::Body::Moon, obs.epoch, leaps());
        auto sun = ephemeris_de440s().geocentric_state(odl::eph::Body::Sun, obs.epoch, leaps());
        REQUIRE(moon.has_value());
        REQUIRE(sun.has_value());
        p.tide_gcrs_m = solid_tide_step1_degree2(sk->position_m, odl::metres_from_km(moon->position()), odl::metres_from_km(sun->position()), tide);
        p.centre_m = range_effect_of_displacement(p.ghat, p.tide_gcrs_m);

        EnvelopeTerms& t = p.terms;
        t.u_h = 0.030;                                              // rule 1: the helper's stated uncertainty (frozen 30 mm)
        t.pole_tide = pole_tide_term_m(e);                          // rule 2
        t.ocean = 0.100;                                            // rule 3: TN36 chapter 7 §7.1.2
        t.orbit = env.orbit_max_m;                                  // rule 4
        t.com = env.com_term_mm * 1e-3;                             // rule 5
        t.dhf = (env.dhf.value + env.dhf.sigma) * 1e-3;             // rule 6: 4.8 + 3.8 mm, unsigned
        t.ecc = env.ecc_norm_mm * 1e-3;                             // rule 7: unprojected
        t.ztd = zenith_delay_term_m(0.5 * (a.mapping_up + a.mapping_down));   // rule 8
        t.gradient = gradient_term_m(e);
        t.mapping = mapping_function_term_m(e);
        t.eop_ut1 = orientation_ut1_term_m(env.sigma_ut1_s);        // rule 9, corrected
        t.eop_pole = orientation_pole_term_m(env.sigma_pole_arcsec);
        REQUIRE(rec.remaining_fields.size() >= 3);                  // rule 10: record 11 = window, raw count, RMS ps, ...
        const std::string& count_text = rec.remaining_fields[1];
        const std::string& rms_text = rec.remaining_fields[2];
        REQUIRE(count_text != "na");                                // a term the sources do not size is listed as unsized, not set to zero
        REQUIRE(rms_text != "na");
        t.np_own = normal_point_term_m(std::stod(rms_text), std::stod(count_text));
        p.w_m = t.sum();
        p.orbit_here_m = orbit_here[i];
        env.points.push_back(p);
    }
    return env;
}

// ---- the assembly defects ----------------------------------------------------------------------------------------------------------------------------------------------------

struct DefectRow {
    std::string name;
    std::string how;
    std::vector<std::optional<double>> delta_m;       ///< per point: the difference of two modelled ranges; nullopt if the defective model REFUSED
    std::vector<std::string> refusals;                ///< per point, the refusal's id when delta_m is empty
};

inline std::optional<double> modelled_range_m(const RangeObservation& obs, const StationTrack& station, std::string* refusal) {
    auto m = model_range(obs, station, target());
    if (!m) {
        *refusal = std::string(m.error().id);
        return std::nullopt;
    }
    return m->range_m();
}

inline std::vector<DefectRow> assembly_defects() {
    const PassChoice& pc = chosen();
    const std::size_t n = pc.observations.size();
    std::vector<DefectRow> rows(8);
    rows[0] = {"centre-of-mass sign", "the correction added where the model subtracts it: range(delta = 0) + delta - range(delta)", {}, {}};
    rows[1] = {"station velocity omitted", "the system reference point of the pass epoch less (marker(t) - marker(2015-01-01)): the station at its SLRF2020 reference-epoch position", {}, {}};
    rows[2] = {"eccentricity omitted", "the marker in place of the system reference point", {}, {}};
    rows[3] = {"wrong epoch event", "the tag (event 2, ground transmit) read as the bounce time (event 1)", {}, {}};
    rows[4] = {"troposphere missing", "minus the mean of the two legs' applied delays (first order)", {}, {}};
    rows[5] = {"troposphere doubled", "plus the mean of the two legs' applied delays (first order)", {}, {}};
    rows[6] = {"Earth's rotation during the light time omitted", "the station frozen in the GCRS at its tag-epoch position, velocity zero", {}, {}};
    rows[7] = {"UTC taken for TT", "the tag's calendar fields read as TT: the epoch 69.184 s earlier", {}, {}};
    for (DefectRow& r : rows) {
        r.delta_m.assign(n, std::nullopt);
        r.refusals.assign(n, "");
    }
    auto zero_com = SphericalCentreOfMass::make(kLageos1Id, 0.0, "a zero correction, the centre-of-mass sign defect's reference run (MEAS-A-101)");
    REQUIRE(zero_com.has_value());
    const Epoch t_ref = utc(2015, 1, 1, 0, 0, 0.0);

    for (std::size_t i = 0; i < n; ++i) {
        const RangeObservation& obs = pc.observations[i];
        const EarthFixedStation station(obs.site.srp_itrs_m, obs.site.srp_geodetic, orientation());
        auto nominal = model_range(obs, station, target());
        REQUIRE(nominal.has_value());
        const double r0 = nominal->range_m();
        std::string why;
        auto put = [&](std::size_t k, std::optional<double> other) {
            if (other) rows[k].delta_m[i] = *other - r0;
            else rows[k].refusals[i] = why;
        };
        {   // (1) the centre-of-mass sign
            RangeObservation o = obs;
            o.com = *zero_com;
            auto r = modelled_range_m(o, station, &why);
            put(0, r ? std::optional<double>(*r + obs.com.metres()) : std::nullopt);
        }
        {   // (2) the station velocity omitted
            auto ref = real_registry().site(obs.site.sod, t_ref);
            REQUIRE(ref.has_value());
            const Vec3 shift = obs.site.marker_itrs_m - ref->marker_itrs_m;          // v (t - t_ref)
            const EarthFixedStation st(obs.site.srp_itrs_m - shift, obs.site.srp_geodetic, orientation());
            put(1, modelled_range_m(obs, st, &why));
        }
        {   // (3) the eccentricity omitted
            const EarthFixedStation st(obs.site.marker_itrs_m, obs.site.marker_geodetic, orientation());
            put(2, modelled_range_m(obs, st, &why));
        }
        {   // (4) the wrong epoch event
            RangeObservation o = obs;
            o.event = obs.event == EpochEvent::GroundTransmit ? EpochEvent::Bounce : EpochEvent::GroundTransmit;
            put(3, modelled_range_m(o, station, &why));
        }
        {   // (5), (6) the troposphere
            const double atm = 0.5 * (nominal->applied().up.delay_atm_m + nominal->applied().down.delay_atm_m);
            rows[4].delta_m[i] = -atm;
            rows[5].delta_m[i] = atm;
        }
        {   // (7) the Earth's rotation during the light time omitted
            auto sk = station.at(obs.epoch);
            REQUIRE(sk.has_value());
            const FrozenStation frozen(*sk);
            put(6, modelled_range_m(obs, frozen, &why));
        }
        {   // (8) UTC taken for TT
            RangeObservation o = obs;
            auto tt = Epoch::from_calendar(TimeScale::TT, utc_calendar_of(obs.epoch, 9), leaps());
            REQUIRE(tt.has_value());
            o.epoch = *tt;
            put(7, modelled_range_m(o, station, &why));
        }
    }
    return rows;
}

/// ρ_i = |Δ_i| / w_i, the classes of §8.9's second set (f).
enum class Seen { ForCertain, MayBe, Cannot, Refused };
inline Seen classify(std::optional<double> delta_m, double w_m) {
    if (!delta_m) return Seen::Refused;
    const double rho = std::abs(*delta_m) / w_m;
    return rho > 2.0 ? Seen::ForCertain : rho > 1.0 ? Seen::MayBe : Seen::Cannot;
}
inline const char* name_of(Seen s) {
    switch (s) {
        case Seen::ForCertain: return "seen for certain";
        case Seen::MayBe: return "may be seen";
        case Seen::Cannot: return "cannot be seen";
        case Seen::Refused: return "refused by the model";
    }
    return "?";
}

}  // namespace odl::measmod::testing::g5
