// orbex_noon_check.cpp — TYAW-Q-005/Q-006's own evidence, reproducible on demand.
//
// NOT part of the automatic gate, for the SAME two reasons `orbex_shape_e_
// check.cpp` is not (tools/orbex_shape_e_check.cpp's own header comment):
// scope (one satellite, one day) and closure.
//
// WHAT THIS PROGRAM DOES: finds a satellite's own real noon-turn onset
// (where its own real attitude first departs from nominal by more than
// 0.01 deg) and catch-up (where it last returns below that), at every real
// noon crossing in the given day's ATT file, and prints each one against
// TYAW-R-002/R-003's own IMPLEMENTED prediction at that SAME real beta --
// found by bisecting the real `odl::attitude::gps_yaw_attitude` call
// itself, not a reimplementation, so this program always tests whatever the
// production law currently is, not a frozen assumption about its shape.
//
// HISTORY (PROVENANCE.md §30.12): first run (day 102, 2023-04-12, IIF)
// found CODE's own file matching the then-implemented LAG law's own width
// to 2-3% but with the centre mirrored -- CODE's own noon turn LEADS
// (leaves nominal early, merges where the nominal rate falls back to the
// hardware limit) rather than lagging. TYAW-R-003 changed accordingly
// (`evaluate_turn_lead`, `attitude.cpp`); re-run after the fix, both
// crossings now match to <0.1 deg (well inside the quarter-tolerance
// criterion). TYAW-R-002 (II/IIA, IIR) is UNCHANGED -- `KOUBA09`'s own
// words state the lag explicitly, settling it independent of any run of
// this program (PROVENANCE.md §30.12's own G05/IIR-M control).
//
// PINNED SOURCE FILES:
//
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20231020000_01D_05M_ORB.SP3.gz
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20231020000_01D_30S_ATT.OBX.gz
//   sha256sum COD0MGXFIN_20231020000_01D_05M_ORB.SP3.gz    # 921644c40cf5cfc2758236996c6783c9f97de9f70ad369f1ba8a68c2c8c06309
//   sha256sum COD0MGXFIN_20231020000_01D_30S_ATT.OBX.gz    # 85e0dacaa7f281aa83863940e5e3a38cd671ac97cce7c079f7b00dfd53eb9c7e
//   gunzip -k COD0MGXFIN_20231020000_01D_05M_ORB.SP3.gz
//   gunzip -k COD0MGXFIN_20231020000_01D_30S_ATT.OBX.gz
//
// LICENCE BASIS: same as `orbex_shape_e_check.cpp`'s own header (IGS's own
// open-data terms of use, PROVENANCE.md §30.11).
//
// BUILD (from the repo root, after `cmake --build build`):
//
//   g++ -std=c++20 -O2 -isystem build/_deps/tl-expected-src/include \
//       -I modules/core/include -I modules/time/include -I modules/eop/include \
//       -I modules/ephemerides/include -I modules/frames/include -I modules/attitude/include \
//       -I build/_deps/calceph-src/include -I build/_deps/erfa-src/src \
//       tools/orbex_noon_check.cpp \
//       build/modules/attitude/libodl_attitude.a build/modules/frames/libodl_frames.a \
//       build/modules/eop/libodl_eop.a build/modules/ephemerides/libodl_ephemerides.a \
//       build/modules/time/libodl_time.a build/_deps/calceph-build/src/libcalceph.a \
//       build/liberfa.a -o /tmp/orbex_noon_check
//
// RUN:
//
//   /tmp/orbex_noon_check COD0MGXFIN_20231020000_01D_05M_ORB.SP3 \
//                         COD0MGXFIN_20231020000_01D_30S_ATT.OBX \
//                         <data/cache>/iers-leap-seconds/Leap_Second.dat \
//                         <data/cache>/eop-c04-20/eopc04.1962-now \
//                         <data/cache>/de440s-spk/de440s.bsp

#include <odl/attitude/attitude.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/transform.hpp>
#include <odl/frames/vector.hpp>
#include <odl/io/sp3.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace odl;

namespace {

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) { std::cerr << "cannot open " << p << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
Vec3 normalized(const Vec3& v) { double n = v.norm(); return Vec3{v.x / n, v.y / n, v.z / n}; }
constexpr double kDeg = M_PI / 180.0;
constexpr double kMuDotDegS = 0.00836;

struct Sp3Row { int y, mo, d, h, mi; double sec; Vec3 r_ecef_km; };
struct AttRow { int y, mo, d, h, mi; double sec; double q0, q1, q2, q3; };

// L6 step 1: through odl::io's own one SP3 reader now (SPEC-io-formats.md),
// not an ad hoc parser -- the predecessor's own two SP3 defects (an interval
// read from the wrong field, a fixed header length skipped) have no second
// place to live here. Sp3Row's own shape is unchanged, so nothing below this
// function needed to change. This tool's own PRN was always hardcoded "G01".
std::vector<Sp3Row> read_sp3(const std::string& path) {
    auto parsed = odl::io::read_sp3(slurp(path));
    if (!parsed.has_value()) {
        std::cerr << "SP3 (" << path << "): " << parsed.error().id << " " << parsed.error().message << "\n";
        std::exit(1);
    }
    std::vector<Sp3Row> rows;
    for (const auto& epoch : parsed->epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == "G01") {
                rows.push_back({epoch.epoch.year, epoch.epoch.month, epoch.epoch.day,
                                epoch.epoch.hour, epoch.epoch.minute, epoch.epoch.second,
                                Vec3{sat.position.x_km, sat.position.y_km, sat.position.z_km}});
            }
        }
    }
    return rows;
}
std::vector<AttRow> read_att(const std::string& path) {
    std::vector<AttRow> rows;
    std::ifstream f(path);
    std::string line;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double sec = 0.0;
    while (std::getline(f, line)) {
        if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
            std::istringstream ss(line.substr(2));
            ss >> y >> mo >> d >> h >> mi >> sec;
        } else if (line.find("ATT G01") != std::string::npos && line.find("ATT G01") < 4) {
            auto pos = line.find("ATT G01") + 7;
            std::istringstream ss(line.substr(pos));
            int n;
            double q0, q1, q2, q3;
            ss >> n >> q0 >> q1 >> q2 >> q3;
            rows.push_back({y, mo, d, h, mi, sec, q0, q1, q2, q3});
        }
    }
    return rows;
}

// Bisects the production gps_yaw_attitude call itself (not a
// reimplementation) for TYAW-R-002/R-003's own onset/catch-up boundary near mu=180,
// at a stated real beta -- the same synthetic-fixture construction
// attitude_tests.cpp's own fixture_at uses (PROVENANCE.md Sec.30.14's own
// sign, matching mu_rad's fix -- an earlier version of this copy predated
// that fix and used the opposite sin(mu) sign).
struct Fix { Vec3 r, v, s; };
Fix fixture_at(double beta_deg, double mu_deg) {
    double beta = beta_deg * kDeg, mu = mu_deg * kDeg;
    Vec3 n_hat{0, 0, 1}, e0{1, 0, 0};
    Vec3 e1 = n_hat.cross(e0);
    Vec3 r_hat = std::cos(mu) * e0 + std::sin(mu) * e1;
    Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {26561e3 * r_hat, 3000.0 * t_hat, s_hat};
}
bool predicted_active(double beta_deg, double mu_deg, const attitude::HardwareYawRates& rates) {
    auto f = fixture_at(beta_deg, mu_deg);
    auto turn = attitude::gps_yaw_attitude(f.r, f.v, f.s, attitude::GpsBlock::IIF, rates);
    auto nom = attitude::nominal_yaw_steering(f.r, f.s);
    if (!turn.has_value() || !nom.has_value()) return false;
    double d = 0;
    for (int i = 0; i < 3; ++i) d = std::max(d, std::abs(turn->r[0][i] - nom->r[0][i]));
    return d > 1e-6;
}
double predicted_boundary(double beta_deg, double lo, double hi, const attitude::HardwareYawRates& rates) {
    bool lo_active = predicted_active(beta_deg, lo, rates);
    for (int i = 0; i < 60; ++i) {
        double mid = (lo + hi) / 2;
        if (predicted_active(beta_deg, mid, rates) == lo_active) lo = mid; else hi = mid;
    }
    return (lo + hi) / 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: orbex_noon_check <sp3> <att> <leap_seconds_dat> <eop_c04> <de440s_bsp>\n";
        return 1;
    }
    auto leaps = time::LeapTable::parse(slurp(argv[3]), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap: " << leaps.error().message << "\n"; return 1; }
    auto c04 = eop::EopSeries::load_c04(slurp(argv[4]), eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; return 1; }
    auto ephem = eph::Ephemeris::open({argv[5]}, {});
    if (!ephem.has_value()) { std::cerr << "ephemeris: " << ephem.error().message << "\n"; return 1; }

    auto make_epoch = [&](int y, int mo, int d, int h, int mi, double sec) {
        time::Calendar c; c.year = y; c.month = mo; c.day = d; c.hour = h; c.minute = mi; c.second = sec;
        auto e = time::Epoch::from_calendar(time::TimeScale::GPS, c, *leaps);
        if (!e.has_value()) { std::cerr << "epoch: " << e.error().message << "\n"; std::exit(1); }
        return *e;
    };
    auto ecef_to_gcrs = [&](const time::Epoch& t, const Vec3& v_ecef_km) -> Vec3 {
        auto eop_rec = c04->at(t, eop::EopPolicy{});
        if (!eop_rec.has_value()) { std::cerr << "eop: " << eop_rec.error().message << "\n"; std::exit(1); }
        frames::ItrsState itrs{t, v_ecef_km, Vec3{0, 0, 0}};
        auto g = frames::to_gcrs(itrs, *eop_rec, *leaps);
        if (!g.has_value()) { std::cerr << "to_gcrs: " << g.error().message << "\n"; std::exit(1); }
        return g->position();
    };

    auto sp3 = read_sp3(argv[1]);
    auto att = read_att(argv[2]);
    if (sp3.empty() || att.empty()) { std::cerr << "no G01 records (sp3=" << sp3.size() << " att=" << att.size() << ")\n"; return 1; }

    time::Epoch t0 = make_epoch(sp3[0].y, sp3[0].mo, sp3[0].d, sp3[0].h, sp3[0].mi, sp3[0].sec);
    std::vector<double> sp3_t(sp3.size());
    std::vector<Vec3> sp3_r(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        auto ti = make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec);
        sp3_t[i] = ti.tai_seconds() - t0.tai_seconds();
        sp3_r[i] = ecef_to_gcrs(ti, sp3[i].r_ecef_km);
    }
    std::vector<Vec3> sp3_v(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        std::size_t im = i == 0 ? 0 : i - 1, ip = i + 1 == sp3.size() ? i : i + 1;
        sp3_v[i] = (1.0 / (sp3_t[ip] - sp3_t[im])) * (sp3_r[ip] - sp3_r[im]);
    }

    const attitude::HardwareYawRates rates{0.11, 0.0, 0.0, 0.0};

    struct S { double t_s, mu_deg, beta_deg, diff_deg; };
    std::vector<S> near_noon;
    std::size_t sp3_idx = 0;
    for (const auto& a : att) {
        auto ta = make_epoch(a.y, a.mo, a.d, a.h, a.mi, a.sec);
        double ta_s = ta.tai_seconds() - t0.tai_seconds();
        while (sp3_idx + 1 < sp3.size() && sp3_t[sp3_idx + 1] < ta_s) ++sp3_idx;
        std::size_t i0 = sp3_idx, i1 = sp3_idx + 1 < sp3.size() ? sp3_idx + 1 : sp3_idx;
        double frac = i1 == i0 ? 0.0 : (ta_s - sp3_t[i0]) / (sp3_t[i1] - sp3_t[i0]);
        Vec3 r_gcrs = sp3_r[i0] + frac * (sp3_r[i1] - sp3_r[i0]);
        Vec3 v_gcrs = sp3_v[i0] + frac * (sp3_v[i1] - sp3_v[i0]);
        auto sun = ephem->geocentric_state(eph::Body::Sun, ta, *leaps);
        if (!sun.has_value()) { std::cerr << "sun: " << sun.error().message << "\n"; return 1; }
        Vec3 sun_dir_km = sun->position() - (1.0 / 1000.0) * r_gcrs;
        Vec3 s_hat = normalized(sun_dir_km);
        Vec3 r_hat = normalized(r_gcrs), n_hat = normalized(r_gcrs.cross(v_gcrs)), t_hat = n_hat.cross(r_hat);
        double beta_deg = std::asin(std::clamp(s_hat.dot(n_hat), -1.0, 1.0)) / kDeg;
        Vec3 u_mid = -1.0 * normalized(s_hat - s_hat.dot(n_hat) * n_hat);
        // negated: matches attitude.cpp's own mu_rad fix, PROVENANCE.md Sec.30.14
        double mu_deg = -std::atan2(t_hat.dot(u_mid), r_hat.dot(u_mid)) / kDeg;
        double mu_wrapped = mu_deg < 0 ? mu_deg + 360.0 : mu_deg;
        if (mu_wrapped < 150.0 || mu_wrapped > 210.0) continue;

        Vec3 xb_ecef{1 - 2 * (a.q2 * a.q2 + a.q3 * a.q3), 2 * (a.q1 * a.q2 - a.q0 * a.q3), 2 * (a.q1 * a.q3 + a.q0 * a.q2)};
        Vec3 xb_gcrs = ecef_to_gcrs(ta, xb_ecef);
        double real_psi = std::atan2(-xb_gcrs.dot(n_hat), -xb_gcrs.dot(t_hat)) / kDeg;
        auto nom = attitude::nominal_yaw_steering(r_gcrs, sun_dir_km);
        if (!nom.has_value()) continue;
        Vec3 xn{nom->r[0][0], nom->r[0][1], nom->r[0][2]};
        double nom_psi = std::atan2(-xn.dot(n_hat), -xn.dot(t_hat)) / kDeg;
        double diff = real_psi - nom_psi;
        while (diff > 180) diff -= 360;
        while (diff < -180) diff += 360;
        near_noon.push_back({ta_s, mu_wrapped, beta_deg, diff});
    }

    // Split into contiguous crossings (a gap in t_s of more than 1 hour
    // starts a new one), find each one's own onset/catch-up by threshold
    // crossing, report against the currently-implemented law's own bisected prediction at the
    // SAME real beta.
    std::size_t i = 0;
    int crossing_num = 0;
    while (i < near_noon.size()) {
        std::size_t j = i;
        while (j + 1 < near_noon.size() && near_noon[j + 1].t_s - near_noon[j].t_s < 3600.0) ++j;
        // onset: first index (scanning i..j) where |diff| > 0.01
        std::optional<double> onset_mu, catchup_mu;
        double beta_sum = 0; int beta_n = 0;
        for (std::size_t k = i; k <= j; ++k) {
            if (std::abs(near_noon[k].diff_deg) > 0.01 && !onset_mu.has_value() && k > i) {
                double f = (0.01 - std::abs(near_noon[k - 1].diff_deg)) /
                           (std::abs(near_noon[k].diff_deg) - std::abs(near_noon[k - 1].diff_deg));
                onset_mu = near_noon[k - 1].mu_deg + f * (near_noon[k].mu_deg - near_noon[k - 1].mu_deg);
            }
            if (onset_mu.has_value() && !catchup_mu.has_value() && std::abs(near_noon[k].diff_deg) < 0.01 && k > i) {
                double f = (std::abs(near_noon[k - 1].diff_deg) - 0.01) /
                           (std::abs(near_noon[k - 1].diff_deg) - std::abs(near_noon[k].diff_deg));
                catchup_mu = near_noon[k - 1].mu_deg + f * (near_noon[k].mu_deg - near_noon[k - 1].mu_deg);
                beta_sum = 0; beta_n = 0;
                for (std::size_t m = i; m <= j; ++m)
                    if (near_noon[m].mu_deg > std::min(*onset_mu, *catchup_mu) &&
                        near_noon[m].mu_deg < std::max(*onset_mu, *catchup_mu)) { beta_sum += near_noon[m].beta_deg; ++beta_n; }
                break;
            }
        }
        if (onset_mu.has_value() && catchup_mu.has_value() && beta_n > 0) {
            double beta_deg = beta_sum / beta_n;
            double real_centre = (*onset_mu + *catchup_mu) / 2.0 - 180.0;
            double real_width = std::abs(*catchup_mu - *onset_mu);
            double pred_onset = predicted_boundary(beta_deg, 170.0, 180.0, rates);
            double pred_catchup = predicted_boundary(beta_deg, 190.0, 180.0, rates);
            double pred_centre = (pred_onset + pred_catchup) / 2.0 - 180.0;
            double pred_width = pred_catchup - pred_onset;
            double tol = std::abs(pred_centre) / 4.0;
            double miss = std::abs(real_centre - pred_centre);
            ++crossing_num;
            std::cout << "crossing " << crossing_num << ": beta=" << beta_deg
                      << "  predicted centre/width=" << pred_centre << "/" << pred_width
                      << "  real centre/width=" << real_centre << "/" << real_width
                      << "  quarter-tolerance=" << tol << "  miss=" << miss
                      << (miss <= tol ? "  MATCHES the implemented law" : "  DOES NOT MATCH (discrepancy, not a model change)")
                      << "\n";
        }
        i = j + 1;
    }
    if (crossing_num == 0) { std::cerr << "no complete noon crossing found in this file's own near-noon window\n"; return 1; }
    return 0;
}
