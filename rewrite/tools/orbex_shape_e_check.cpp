// orbex_shape_e_check.cpp — TYAW-A-009's own evidence, reproducible on demand.
//
// NOT part of the automatic gate (tools/ci.sh does not build or run this).
// The discrimination this program performs (Shape E vs Shape F for IIF's own
// night side, SPEC-thrust-yaw.md §4.3) is CLOSED -- Shape E won decisively,
// PROVENANCE.md §30.8/§30.10 record the verdict. Ongoing regression
// protection for the MATH is TYAW-A-004b (a pure closed-form check, no
// external dependency, run by every `ctest`). This program exists so a
// future maintainer can RE-VERIFY the empirical corroboration against real
// CODE MGEX data on demand, not so every CI run re-fetches historical GPS
// attitude data from an external server for a question this tree has
// already settled.
//
// WHY REPRODUCIBLE RATHER THAN GATED (manifest.json + tools/fetch.py, the
// pattern every other external input in this tree follows, PROVENANCE.md
// §30.11): two independent reasons, not one. (1) SCOPE -- these files are
// specific to one historical validation exercise (one satellite, one day),
// not general infrastructure another module would reuse the way
// de440s.bsp or the EOP series are; the ATT file alone is 35 MB decompressed
// for a question already answered. (2) The question itself is CLOSED, so
// there is no ongoing regression this file's own presence in the manifest
// would protect that TYAW-A-004b does not already cover more cheaply.
// Reproducibility (fetch-by-hash outside CI) is preserved by the pin below,
// not by tracking these files in the manifest's own every-run verify path.
//
// LICENCE BASIS (checked, not assumed -- PROVENANCE.md §30.11 has the full
// record): the International GNSS Service's own "Data and Product
// Disclaimer and Terms of Use" (5 August 2020, fetched from
// https://igs.org/wp-content/uploads/2020/09/IGS-Data-and-Product-Disclaimer-and-Terms-of-Use-200805.pdf)
// states: "The IGS products and station data are provided openly for the
// benefit of all scientific, educational, and commercial users. For 25
// years, IGS data and products have been made openly available for use
// without restriction, and continue to be offered free of cost or
// obligation," subject only to an attribution requirement ("users agree to
// appropriately cite and attribute these resources to providers and their
// sponsors, acknowledgment of IGS and its contributing organizations").
// CODE (AIUB, University of Bern) is a founding IGS Analysis Center; the
// files below are CODE's own MGEX pilot-project contribution, in IGS-
// standard formats, served from CODE's own institutional bucket. Attribution:
// "Center for Orbit Determination in Europe (CODE)," MGEX orbits/attitude
// for 2023-098, DOI 10.7892/boris.75882.3 (from the SP3 file's own header).
//
// PINNED SOURCE FILES (fetch, verify, decompress before running this
// program -- it reads the decompressed text form, not the .gz):
//
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20230980000_01D_05M_ORB.SP3.gz
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20230980000_01D_30S_ATT.OBX.gz
//   sha256sum COD0MGXFIN_20230980000_01D_05M_ORB.SP3.gz    # 02d2c6a43f2cf0a2939ecccc6f4ff874a866fda8756ed9c2d9b473a22c7b7211
//   sha256sum COD0MGXFIN_20230980000_01D_30S_ATT.OBX.gz    # a8a8538399f37f5cd2663fa51d5ccdf1e5f3c786db489e8d403449be258d6286
//   gunzip -k COD0MGXFIN_20230980000_01D_05M_ORB.SP3.gz
//   gunzip -k COD0MGXFIN_20230980000_01D_30S_ATT.OBX.gz
//
// BUILD (against this tree's own already-built static libraries -- run
// `cmake --build build` first):
//
//   g++ -std=c++23 -O2 -I include \
//       -I modules/core/include -I modules/time/include -I modules/eop/include \
//       -I modules/ephemerides/include -I modules/frames/include \
//       -I modules/attitude/include -I modules/dynamics/include \
//       -I build/_deps/calceph-src/include -I build/_deps/erfa-src/src \
//       tools/orbex_shape_e_check.cpp \
//       build/modules/attitude/libodl_attitude.a \
//       build/modules/frames/libodl_frames.a \
//       build/modules/eop/libodl_eop.a \
//       build/modules/ephemerides/libodl_ephemerides.a \
//       build/modules/time/libodl_time.a \
//       build/_deps/calceph-build/src/libcalceph.a \
//       build/liberfa.a \
//       -o /tmp/orbex_shape_e_check
//
// RUN:
//
//   /tmp/orbex_shape_e_check COD0MGXFIN_20230980000_01D_05M_ORB.SP3 \
//                            COD0MGXFIN_20230980000_01D_30S_ATT.OBX \
//                            <path-to-data/cache>/iers-leap-seconds/Leap_Second.dat \
//                            <path-to-data/cache>/eop-c04-20/eopc04.1962-now \
//                            <path-to-data/cache>/de440s-spk/de440s.bsp
//
// Exit 0 and "PASS" if every sampled point inside the shadow window, both
// midnight crossings, has |residual| <= 0.15 deg (TYAW-A-009's own frozen
// tolerance, PROVENANCE.md §30.10 -- roughly 2.3x the largest residual
// actually observed, 0.065/0.061 deg, when this program was first run).
// Exit 1 otherwise.

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
#include <optional>
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

Vec3 normalized(const Vec3& v) {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

constexpr double kShadowHalfAngleDeg = 13.5;  // TYAW-R-001's own E_sh, borrowed for IIF (TYAW-R-003)
constexpr double kDeg = M_PI / 180.0;

struct Sp3Row { int y, mo, d, h, mi; double sec; Vec3 r_ecef_km; };
struct AttRow { int y, mo, d, h, mi; double sec; double q0, q1, q2, q3; };

// L6 step 1: through odl::io's own one SP3 reader now (SPEC-io-formats.md),
// not an ad hoc parser -- the predecessor's own two SP3 defects (an interval
// read from the wrong field, a fixed header length skipped) have no second
// place to live here. Sp3Row's own shape is unchanged, so nothing below this
// function needed to change.
std::vector<Sp3Row> read_sp3(const std::string& path, const std::string& prn) {
    auto parsed = odl::io::read_sp3(slurp(path));
    if (!parsed.has_value()) {
        std::cerr << "SP3 (" << path << "): " << parsed.error().id << " " << parsed.error().message << "\n";
        std::exit(1);
    }
    std::vector<Sp3Row> rows;
    for (const auto& epoch : parsed->epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == prn) {
                rows.push_back({epoch.epoch.year, epoch.epoch.month, epoch.epoch.day,
                                epoch.epoch.hour, epoch.epoch.minute, epoch.epoch.second,
                                Vec3{sat.position.x_km, sat.position.y_km, sat.position.z_km}});
            }
        }
    }
    return rows;
}

// --- ORBEX: epoch lines "## Y MO D H MI SEC N", record lines " ATT <prn>  4  q0 q1 q2 q3" ---
std::vector<AttRow> read_att(const std::string& path, const std::string& prn) {
    std::vector<AttRow> rows;
    std::ifstream f(path);
    std::string line;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double sec = 0.0;
    const std::string marker = "ATT " + prn;
    while (std::getline(f, line)) {
        if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
            std::istringstream ss(line.substr(2));
            ss >> y >> mo >> d >> h >> mi >> sec;
        } else if (line.find(marker) != std::string::npos && line.find(marker) < 4) {
            auto pos = line.find(marker) + marker.size();
            std::istringstream ss(line.substr(pos));
            int n;
            double q0, q1, q2, q3;
            ss >> n >> q0 >> q1 >> q2 >> q3;
            rows.push_back({y, mo, d, h, mi, sec, q0, q1, q2, q3});
        }
    }
    return rows;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: orbex_shape_e_check <sp3> <att> <leap_seconds_dat> <eop_c04> <de440s_bsp>\n";
        return 1;
    }
    const std::string sp3_path = argv[1], att_path = argv[2];
    const std::string leap_path = argv[3], eop_path = argv[4], bsp_path = argv[5];

    auto leaps = time::LeapTable::parse(slurp(leap_path), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap table: " << leaps.error().message << "\n"; return 1; }

    auto c04 = eop::EopSeries::load_c04(slurp(eop_path), eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; return 1; }

    auto ephem = eph::Ephemeris::open({bsp_path}, {});
    if (!ephem.has_value()) { std::cerr << "ephemeris: " << ephem.error().message << "\n"; return 1; }

    auto make_epoch = [&](int y, int mo, int d, int h, int mi, double sec) {
        time::Calendar c;
        c.year = y; c.month = mo; c.day = d; c.hour = h; c.minute = mi; c.second = sec;
        auto e = time::Epoch::from_calendar(time::TimeScale::GPS, c, *leaps);
        if (!e.has_value()) { std::cerr << "epoch: " << e.error().message << "\n"; std::exit(1); }
        return *e;
    };
    auto ecef_to_gcrs = [&](const time::Epoch& t, const Vec3& v_ecef_km) -> Vec3 {
        auto eop_rec = c04->at(t, eop::EopPolicy{});
        if (!eop_rec.has_value()) { std::cerr << "eop at: " << eop_rec.error().message << "\n"; std::exit(1); }
        const frames::ItrsState itrs{t, v_ecef_km, Vec3{0, 0, 0}};
        auto gcrs = frames::to_gcrs(itrs, *eop_rec, *leaps);
        if (!gcrs.has_value()) { std::cerr << "to_gcrs: " << gcrs.error().message << "\n"; std::exit(1); }
        return gcrs->position();
    };

    auto sp3 = read_sp3(sp3_path, "G01");
    auto att = read_att(att_path, "G01");
    if (sp3.empty() || att.empty()) {
        std::cerr << "no G01 records found (sp3=" << sp3.size() << " att=" << att.size() << ")\n";
        return 1;
    }

    time::Epoch t0 = make_epoch(sp3[0].y, sp3[0].mo, sp3[0].d, sp3[0].h, sp3[0].mi, sp3[0].sec);
    std::vector<double> sp3_t_s(sp3.size());
    std::vector<Vec3> sp3_r_gcrs(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        time::Epoch ti = make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec);
        sp3_t_s[i] = ti.tai_seconds() - t0.tai_seconds();
        sp3_r_gcrs[i] = ecef_to_gcrs(ti, sp3[i].r_ecef_km);
    }
    std::vector<Vec3> sp3_v_gcrs(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        std::size_t im = (i == 0) ? 0 : i - 1;
        std::size_t ip = (i + 1 == sp3.size()) ? i : i + 1;
        const double dt = sp3_t_s[ip] - sp3_t_s[im];
        sp3_v_gcrs[i] = (1.0 / dt) * (sp3_r_gcrs[ip] - sp3_r_gcrs[im]);  // GCRS already inertial
    }

    const attitude::HardwareYawRates rates{0.11, 0.0, 0.0, 0.0};  // noon only; IIF night is Shape E

    struct Sample { double t_s, mu_deg, beta_deg, real_psi_deg, prod_psi_deg; };
    std::vector<Sample> samples;

    std::size_t sp3_idx = 0;
    for (const auto& a : att) {
        time::Epoch ta = make_epoch(a.y, a.mo, a.d, a.h, a.mi, a.sec);
        double ta_s = ta.tai_seconds() - t0.tai_seconds();
        while (sp3_idx + 1 < sp3.size() && sp3_t_s[sp3_idx + 1] < ta_s) ++sp3_idx;
        std::size_t i0 = sp3_idx, i1 = (sp3_idx + 1 < sp3.size()) ? sp3_idx + 1 : sp3_idx;
        double frac = (i1 == i0) ? 0.0 : (ta_s - sp3_t_s[i0]) / (sp3_t_s[i1] - sp3_t_s[i0]);
        Vec3 r_gcrs = sp3_r_gcrs[i0] + frac * (sp3_r_gcrs[i1] - sp3_r_gcrs[i0]);
        Vec3 v_gcrs = sp3_v_gcrs[i0] + frac * (sp3_v_gcrs[i1] - sp3_v_gcrs[i0]);

        auto sun_state = ephem->geocentric_state(eph::Body::Sun, ta, *leaps);
        if (!sun_state.has_value()) { std::cerr << "sun: " << sun_state.error().message << "\n"; return 1; }
        Vec3 s_hat = normalized(sun_state->position() - (1.0 / 1000.0) * r_gcrs);

        Vec3 r_hat = normalized(r_gcrs);
        Vec3 n_hat = normalized(r_gcrs.cross(v_gcrs));
        Vec3 t_hat = n_hat.cross(r_hat);
        double beta_deg = std::asin(std::clamp(s_hat.dot(n_hat), -1.0, 1.0)) / kDeg;
        Vec3 s_orb_raw = s_hat - s_hat.dot(n_hat) * n_hat;
        Vec3 u_mid = -1.0 * normalized(s_orb_raw);
        // negated: matches attitude.cpp's own mu_rad fix, PROVENANCE.md Sec.30.14
        double mu_deg = -std::atan2(t_hat.dot(u_mid), r_hat.dot(u_mid)) / kDeg;

        if (beta_deg * beta_deg >= kShadowHalfAngleDeg * kShadowHalfAngleDeg) continue;  // outside Shape E's own domain
        double half_width_deg = std::sqrt(kShadowHalfAngleDeg * kShadowHalfAngleDeg - beta_deg * beta_deg);
        if (mu_deg < -half_width_deg || mu_deg > half_width_deg) continue;

        // Real attitude: quaternion -> x_body in ECEF, row 0 of the standard
        // unit-quaternion rotation matrix (this file's own convention is
        // ECEF->body, PROVENANCE.md §30.8's own nadir test).
        Vec3 x_body_ecef{1 - 2 * (a.q2 * a.q2 + a.q3 * a.q3), 2 * (a.q1 * a.q2 - a.q0 * a.q3),
                          2 * (a.q1 * a.q3 + a.q0 * a.q2)};
        Vec3 x_body_gcrs = ecef_to_gcrs(ta, x_body_ecef);
        double real_psi_deg = std::atan2(-x_body_gcrs.dot(n_hat), -x_body_gcrs.dot(t_hat)) / kDeg;

        // Production: the real gps_yaw_attitude call, not a reimplementation.
        auto prod = attitude::gps_yaw_attitude(r_gcrs, v_gcrs, sun_state->position() - (1.0 / 1000.0) * r_gcrs,
                                                attitude::GpsBlock::IIF, rates);
        if (!prod.has_value()) { std::cerr << "gps_yaw_attitude: " << prod.error().message << "\n"; return 1; }
        Vec3 x_body_prod{prod->r[0][0], prod->r[0][1], prod->r[0][2]};
        double prod_psi_deg = std::atan2(-x_body_prod.dot(n_hat), -x_body_prod.dot(t_hat)) / kDeg;

        samples.push_back({ta_s, mu_deg, beta_deg, real_psi_deg, prod_psi_deg});
    }

    if (samples.empty()) {
        std::cerr << "no samples fell inside the shadow window -- wrong day, or G01 wasn't eclipsed\n";
        return 1;
    }

    constexpr double kToleranceDeg = 0.15;  // TYAW-A-009's own frozen tolerance, SPEC-thrust-yaw.md §8
    double max_abs_resid = 0.0, sum_sq = 0.0;
    for (const auto& s : samples) {
        double resid = s.real_psi_deg - s.prod_psi_deg;
        while (resid > 180.0) resid -= 360.0;
        while (resid < -180.0) resid += 360.0;
        max_abs_resid = std::max(max_abs_resid, std::abs(resid));
        sum_sq += resid * resid;
        std::cout << "t_s=" << s.t_s << " mu_deg=" << s.mu_deg << " beta_deg=" << s.beta_deg
                  << " residual_deg=" << resid << "\n";
    }
    double rms = std::sqrt(sum_sq / samples.size());

    std::cout << "\nn_samples=" << samples.size() << " max_abs_residual_deg=" << max_abs_resid
              << " rms_deg=" << rms << " tolerance_deg=" << kToleranceDeg << "\n";

    if (max_abs_resid <= kToleranceDeg) {
        std::cout << "PASS\n";
        return 0;
    }
    std::cout << "FAIL\n";
    return 1;
}
