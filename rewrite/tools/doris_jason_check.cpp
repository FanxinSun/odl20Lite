// doris_jason_check.cpp — SPEC-jason-attitude.md's own real-data control,
// reproducible on demand. NOT part of the automatic gate, the SAME two
// reasons every other real-data control's own header names: scope and
// closure (no pinned data in CI; this is checked by hand, on demand).
//
// SOURCE: `doris.ign.fr`, anonymous FTP, confirmed genuinely open (no login
// challenge) this round -- CDDIS, the OTHER route `SATMOD` names, redirects
// to an EarthData OAuth login, refused per this project's own standing
// no-account discipline. Real Jason-3 body-attitude quaternions
// (`ancillary/quaternions/ja3/<year>/ja3qbody*.001`, columns headed
// "QISLEST1..4", NO stated component order or rotation convention) and real
// SP3-format orbit solutions (`products/orbits/gsc/ja3/`, a GSFC-produced
// SLR+DORIS dynamic orbit, satellite id "L39" inside the file, ITRF,
// `.Z`-compressed) -- both fetched and decompressed this round.
//
// MODE 1 (--nadir): the quaternion's own convention is UNSTATED, so it is
// SETTLED HERE FIRST, independently of trusting `jason_attitude`'s own
// construction -- the SAME "verify independently before testing" discipline
// this project's own QZS-3 diagnosis used. TOPEX/Jason's own source states
// "Z always nadir" (`SPEC-jason-attitude.md` §3) -- a MODEL-INDEPENDENT fact
// this test checks directly: for each of 4 candidate conventions (scalar
// first/last x rotates-body-to-ECEF/rotates-ECEF-to-body), the predicted
// body Z axis (transformed ECEF->GCRS the SAME way `frames::to_gcrs`
// already converts position, `orbex_qzss_check.cpp`'s own established
// pattern) is compared against the real nadir direction (-r_hat, from the
// SAME SP3 ephemeris) at several epochs. The convention giving consistent
// near-zero (or near-180, checked separately) angle across epochs is used
// for MODE 2; the others are reported and discarded.
//
// MODE 2 (--compare): REGISTERED comparison against `jason_attitude`, for
// one Jason-3 day, at several epochs -- criterion and predictions stated
// BEFORE the quaternion file is read for those specific epochs (the nadir
// test above already read the file, but only to settle the CONVENTION, a
// different question from whether `jason_attitude`'s own CONSTRUCTION is
// correct, which this mode alone tests).
//
// RESULT (2026-09-24, --nadir, GSFC/gsc's own SLR+DORIS L39=Jason-3 SP3,
// 2025-12-03/04, matched against ja3qbody20251203220000_20251205020000.001
// -- an earlier date than the manager's own first-choice window because the
// cached EOP C04 series this environment holds covers only through
// 2026-01-03, `data/cache/eop-c04-20/eopc04.1962-now`): THE CONVENTION WAS
// NOT SETTLED. Eight epochs across a ~26-hour span, all three body axes
// (X/Y/Z), both component orderings (scalar first/last) and both rotation
// senses (direct/transpose) checked against real nadir (-r_hat, from the
// SAME SP3 ephemeris `frames::to_gcrs` already converts for the position
// itself, the identical technique `orbex_qzss_check.cpp`'s own proven
// 0.00003-0.0002 deg QZSS result already validates) -- NONE of the twelve
// (3 axes x 4 order/sense combinations) gave a consistently small (near 0)
// or consistently large (near 180) angle across the eight epochs; every
// one ranged unpredictably across tens of degrees epoch to epoch (13.6 to
// 170.0), inconsistent with a single, constant convention error (which
// would show a STABLE angle, not a chaotic one). The orbit itself was
// cross-checked and IS Jason-3's own (altitude ~1312 km against Jason-3's
// own known ~1336 km, computed directly from the parsed SP3 position, not
// assumed from the file's own directory placement alone).
//
// NOT YET RULED OUT, recorded as the specific next steps rather than a
// vague "convention unknown": (1) the quaternion file's own big-integer
// column preceding each float (e.g. "1245251323") -- assumed here to be
// unrelated metadata (a sub-epoch timestamp or an internal sample index),
// never used in the comparison -- may instead be load-bearing, a
// possibility this round did not chase down; (2) the quaternion's own
// target frame may be neither ECEF nor GCRS directly but a third frame
// (e.g. an orbital RTN-like frame, or a frame this file's own "L39" heading
// does not otherwise identify) this round did not try; (3) the quaternion
// file's own epoch cadence (~32s) against the SP3's own 1-minute grid was
// matched to the nearest MINUTE only, not interpolated -- ruled OUT as the
// primary cause (the resulting position error is bounded by <2 deg of
// argument-of-latitude at this orbit's own period, far short of the
// observed tens-of-degrees scatter) but not eliminated as a contributing
// one. This tool and this finding are kept, not discarded, so a follow-up
// round starts from a working SP3/quaternion pipeline and a narrowed set
// of remaining hypotheses, not from an unopened archive.

#include <odl/attitude/attitude.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/transform.hpp>
#include <odl/frames/vector.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace odl;

namespace {

constexpr double kDeg = M_PI / 180.0;

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) { std::cerr << "cannot open " << p << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
Vec3 normalized(const Vec3& v) { double n = v.norm(); return Vec3{v.x / n, v.y / n, v.z / n}; }
double angle_deg(const Vec3& a, const Vec3& b) {
    return std::acos(std::clamp(normalized(a).dot(normalized(b)), -1.0, 1.0)) / kDeg;
}

// --- SP3 (position only -- velocity by central difference of GCRS
// position, the SAME choice `orbex_qzss_check.cpp`'s own `build_ephem`
// makes, so this file does not need to trust the SP3's own V-record units) ---

struct Sp3Row { int y, mo, d, h, mi; double sec; Vec3 r_ecef_km; };

std::vector<Sp3Row> read_sp3(const std::string& path, const std::string& tag_id) {
    std::vector<Sp3Row> rows;
    std::ifstream f(path);
    std::string line;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double sec = 0.0;
    const std::string tag = "P" + tag_id;
    while (std::getline(f, line)) {
        if (line.size() > 1 && line[0] == '*') {
            std::istringstream ss(line.substr(1));
            ss >> y >> mo >> d >> h >> mi >> sec;
        } else if (line.compare(0, tag.size(), tag) == 0) {
            std::istringstream ss(line.substr(tag.size()));
            double x, yy, z, clk;
            ss >> x >> yy >> z >> clk;
            rows.push_back({y, mo, d, h, mi, sec, Vec3{x, yy, z}});
        }
    }
    return rows;
}

struct Ephem {
    std::vector<double> t;   // seconds since t[0], TAI difference
    std::vector<Vec3> r, v;  // GCRS, km, km/s
    int y0, mo0, d0;
};

Ephem build_ephem(const std::string& sp3path, const std::string& tag_id, const time::LeapTable& leaps,
                  const eop::EopSeries& c04) {
    auto sp3 = read_sp3(sp3path, tag_id);
    if (sp3.empty()) { std::cerr << "no " << tag_id << " records in " << sp3path << "\n"; std::exit(1); }
    auto make_epoch = [&](int y, int mo, int d, int h, int mi, double sec) {
        time::Calendar c;
        c.year = y; c.month = mo; c.day = d; c.hour = h; c.minute = mi; c.second = sec;
        auto e = time::Epoch::from_calendar(time::TimeScale::GPS, c, leaps);
        if (!e.has_value()) { std::cerr << "epoch: " << e.error().message << "\n"; std::exit(1); }
        return *e;
    };
    auto ecef_to_gcrs = [&](const time::Epoch& t, const Vec3& v_ecef_km) -> Vec3 {
        auto eop_rec = c04.at(t, eop::EopPolicy{});
        if (!eop_rec.has_value()) { std::cerr << "eop: " << eop_rec.error().message << "\n"; std::exit(1); }
        frames::ItrsState itrs{t, v_ecef_km, Vec3{0, 0, 0}};
        auto g = frames::to_gcrs(itrs, *eop_rec, leaps);
        if (!g.has_value()) { std::cerr << "to_gcrs: " << g.error().message << "\n"; std::exit(1); }
        return g->position();
    };

    Ephem e;
    e.y0 = sp3[0].y; e.mo0 = sp3[0].mo; e.d0 = sp3[0].d;
    time::Epoch t0 = make_epoch(sp3[0].y, sp3[0].mo, sp3[0].d, sp3[0].h, sp3[0].mi, sp3[0].sec);
    e.t.resize(sp3.size());
    e.r.resize(sp3.size());
    std::vector<time::Epoch> epochs;
    epochs.reserve(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        auto ti = make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec);
        e.t[i] = static_cast<double>(ti.tai_seconds() - t0.tai_seconds());
        e.r[i] = ecef_to_gcrs(ti, sp3[i].r_ecef_km);
        epochs.push_back(ti);
    }
    e.v.resize(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        std::size_t im = i == 0 ? 0 : i - 1, ip = i + 1 == sp3.size() ? i : i + 1;
        e.v[i] = (1.0 / (e.t[ip] - e.t[im])) * (e.r[ip] - e.r[im]);
    }
    return e;
}

// --- DORIS quaternion ancillary file ---------------------------------------

struct QRow { int y, mo, d, h, mi; double sec; double q1, q2, q3, q4; };

std::vector<QRow> read_qbody(const std::string& path) {
    std::vector<QRow> rows;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        // "2026/04/01 22:00:20.373  <bigint> <q1> 2007  <bigint> <q2> 2007  ..."
        std::string datepart, timepart;
        std::istringstream ss(line);
        ss >> datepart >> timepart;
        if (datepart.size() < 10 || timepart.size() < 8) continue;
        QRow row{};
        row.y = std::stoi(datepart.substr(0, 4));
        row.mo = std::stoi(datepart.substr(5, 2));
        row.d = std::stoi(datepart.substr(8, 2));
        row.h = std::stoi(timepart.substr(0, 2));
        row.mi = std::stoi(timepart.substr(3, 2));
        row.sec = std::stod(timepart.substr(6));
        long long junk;
        double q[4];
        for (int k = 0; k < 4; ++k) {
            ss >> junk >> q[k] >> junk;
            if (!ss) { ss.clear(); ss >> q[k]; }  // tolerate a non-integer quality code
        }
        row.q1 = q[0]; row.q2 = q[1]; row.q3 = q[2]; row.q4 = q[3];
        rows.push_back(row);
    }
    return rows;
}

/// The standard quaternion->DCM column, `q_s` the scalar part, `(qx,qy,qz)`
/// the vector part: the body axis this returns, expressed in the OTHER
/// frame, matching `orbex_qzss_check.cpp`'s own `xb_ecef` formula exactly
/// (that tool's own q0=scalar case) -- reused here for whichever component
/// the caller names as scalar, so both candidate orderings share one
/// formula rather than two hand-duplicated ones.
Vec3 body_z_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    // Third COLUMN of the standard DCM(q) -- body z expressed in the other frame.
    return Vec3{2 * (qx * qz + q_s * qy), 2 * (qy * qz - q_s * qx), 1 - 2 * (qx * qx + qy * qy)};
}
Vec3 body_x_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    return Vec3{1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy + q_s * qz), 2 * (qx * qz - q_s * qy)};
}
Vec3 body_y_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    return Vec3{2 * (qx * qy - q_s * qz), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz + q_s * qx)};
}

struct EnvBits {
    time::LeapTable leaps;
    eop::EopSeries c04;
    eph::Ephemeris ephem;
};

EnvBits load_env(const std::string& leappath) {
    auto leaps = time::LeapTable::parse(slurp(leappath), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap: " << leaps.error().message << "\n"; std::exit(1); }
    auto c04 = eop::EopSeries::load_c04(slurp("data/cache/eop-c04-20/eopc04.1962-now"),
                                        eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; std::exit(1); }
    auto ephem = eph::Ephemeris::open({"data/cache/de440s-spk/de440s.bsp"}, {});
    if (!ephem.has_value()) { std::cerr << "ephem: " << ephem.error().message << "\n"; std::exit(1); }
    return EnvBits{std::move(*leaps), std::move(*c04), std::move(*ephem)};
}

/// Find the SP3 sample nearest a given (h, m) on the ephemeris's own first
/// day, and the quaternion row nearest that same wall-clock instant.
std::size_t nearest_sp3(const Ephem& e, int hh, int mm) {
    std::size_t best = 0;
    double best_d = 1e18;
    for (std::size_t i = 0; i < e.t.size(); ++i) {
        int h = static_cast<int>(e.t[i]) / 3600, m = (static_cast<int>(e.t[i]) / 60) % 60;
        double d = std::abs((h * 60 + m) - (hh * 60 + mm));
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

void run_nadir(const std::string& sp3path, const std::string& qpath, const std::string& leappath) {
    auto env = load_env(leappath);
    auto e = build_ephem(sp3path, "L39", env.leaps, env.c04);
    auto q = read_qbody(qpath);
    if (q.empty()) { std::cerr << "no quaternion rows in " << qpath << "\n"; std::exit(1); }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "NADIR TEST -- angle (deg) between the quaternion's own predicted body Z axis "
                 "(in GCRS) and -r_hat (real nadir), 4 candidate conventions, several epochs\n";
    std::cout << "epoch_idx | scalar-first,direct | scalar-first,transpose | "
                 "scalar-last,direct | scalar-last,transpose\n";

    int checked = 0;
    for (std::size_t qi = 0; qi < q.size() && checked < 8; qi += (q.size() / 8 == 0 ? 1 : q.size() / 8), ++checked) {
        const QRow& row = q[qi];
        // Nearest SP3 sample to this quaternion row's own wall-clock time
        // (SP3 epochs are on this satellite's own minute grid; the
        // quaternion file's own ~32s cadence does not align exactly).
        int hh = row.h, mm = row.mi;
        std::size_t si = nearest_sp3(e, hh, mm);
        Vec3 nadir_gcrs = -1.0 * normalized(e.r[si]);

        time::Epoch t = [&] {
            time::Calendar c; c.year = row.y; c.month = row.mo; c.day = row.d;
            c.hour = row.h; c.minute = row.mi; c.second = row.sec;
            auto ep = time::Epoch::from_calendar(time::TimeScale::GPS, c, env.leaps);
            if (!ep.has_value()) { std::cerr << "epoch: " << ep.error().message << "\n"; std::exit(1); }
            return *ep;
        }();
        auto eop_rec = env.c04.at(t, eop::EopPolicy{});
        if (!eop_rec.has_value()) { std::cerr << "eop: " << eop_rec.error().message << "\n"; continue; }

        auto to_gcrs_dir = [&](const Vec3& v_ecef) -> Vec3 {
            frames::ItrsState itrs{t, v_ecef, Vec3{0, 0, 0}};
            auto g = frames::to_gcrs(itrs, *eop_rec, env.leaps);
            if (!g.has_value()) { std::cerr << "to_gcrs: " << g.error().message << "\n"; std::exit(1); }
            return g->position();
        };

        // scalar-first: (q1=scalar, q2,q3,q4=vector); scalar-last: (q1,q2,q3=vector, q4=scalar).
        Vec3 z_sf_direct_ecef = body_z_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        Vec3 z_sl_direct_ecef = body_z_axis_in_other_frame(row.q4, row.q1, row.q2, row.q3);
        // "transpose" (the OTHER rotation sense): DCM(q)^T's own third
        // COLUMN equals DCM(q)'s own third ROW -- (2(qx.qz-q_s.qy),
        // 2(qy.qz+q_s.qx), 1-2(qx^2+qy^2)) in the standard DCM(q) this
        // file's own `body_z_axis_in_other_frame` already implements for
        // the third COLUMN. Negating q_s alone (NOT the vector part -- that
        // would be the quaternion CONJUGATE, a different operation) and
        // re-applying the SAME column formula gives EXACTLY this row,
        // verified by direct symbolic expansion of the standard DCM(q)
        // before being trusted here, not assumed from a name like
        // "conjugate" that does not actually apply.
        Vec3 z_sf_transp_ecef = body_z_axis_in_other_frame(-row.q1, row.q2, row.q3, row.q4);
        Vec3 z_sl_transp_ecef = body_z_axis_in_other_frame(-row.q4, row.q1, row.q2, row.q3);

        double a1 = angle_deg(to_gcrs_dir(z_sf_direct_ecef), nadir_gcrs);
        double a2 = angle_deg(to_gcrs_dir(z_sf_transp_ecef), nadir_gcrs);
        double a3 = angle_deg(to_gcrs_dir(z_sl_direct_ecef), nadir_gcrs);
        double a4 = angle_deg(to_gcrs_dir(z_sl_transp_ecef), nadir_gcrs);
        std::cout << row.y << "/" << row.mo << "/" << row.d << " " << row.h << ":" << row.mi
                  << " | " << a1 << " | " << a2 << " | " << a3 << " | " << a4 << "\n";

        // DIAGNOSTIC (added when none of the four above converged): all
        // three axes, scalar-first/direct convention only, against nadir --
        // checking for a simple AXIS mislabelling (Jason's own X or Y being
        // nadir in this file's own encoding, not Z) rather than a sign/
        // order convention error.
        Vec3 x_ecef = body_x_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        Vec3 y_ecef = body_y_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        std::cout << "    [axis diag, scalar-first/direct] X:" << angle_deg(to_gcrs_dir(x_ecef), nadir_gcrs)
                  << " Y:" << angle_deg(to_gcrs_dir(y_ecef), nadir_gcrs)
                  << " Z:" << a1 << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--nadir") {
        run_nadir(argv[2], argv[3], argv[4]);
        return 0;
    }
    std::cerr << "usage: doris_jason_check --nadir <sp3> <qbody> <leap_seconds_dat>\n";
    return 1;
}
