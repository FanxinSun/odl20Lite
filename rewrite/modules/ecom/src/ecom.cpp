// ecom.cpp — SPEC-ecom, ECOM2 (Arnold et al. 2015, `ARN15`).

#include <odl/ecom/ecom.hpp>

#include <cmath>
#include <cstddef>

namespace odl::ecom {
namespace {

constexpr double kMinAxisNorm = 1.0e-9;   ///< matches attitude.hpp's own tight threshold
constexpr double kPositionStep_m = 1.0;   ///< ECOM-R-006's own central-difference step for d(a)/d(r)

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

[[nodiscard]] double component(const Vec3& v, std::size_t i) noexcept {
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}
[[nodiscard]] Vec3 column(const Mat3& m, std::size_t j) noexcept {
    return Vec3{m.r[0][j], m.r[1][j], m.r[2][j]};
}
void set_column(Mat3& m, std::size_t j, const Vec3& v) noexcept {
    m.r[0][j] = v.x; m.r[1][j] = v.y; m.r[2][j] = v.z;
}

// `core/vec3.hpp`'s own Mat3 has no +, - or scalar-* (deliberately, its own
// header says so): every existing user (Drag::accel's own dadr) accumulates
// through r[i][j] directly. These stay local to this file rather than
// widening a shared, frozen-small type for one caller's convenience.
[[nodiscard]] Mat3 mat3_scale(double s, const Mat3& m) noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = s * m.r[i][j];
    return out;
}
[[nodiscard]] Mat3 mat3_add(const Mat3& a, const Mat3& b) noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = a.r[i][j] + b.r[i][j];
    return out;
}
[[nodiscard]] Mat3 zero3() noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = 0.0;
    return out;
}
/// col * row, a 3x3 outer product -- (col_vec)(row_vec)^T.
[[nodiscard]] Mat3 outer(const Vec3& col_vec, const Vec3& row) noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = component(col_vec, i) * component(row, j);
    return out;
}
/// skew(a) * x == a cross x, for every x -- the matrix standing in for
/// "cross with a", so a v-dependent cross product can be chain-ruled.
[[nodiscard]] Mat3 skew(const Vec3& a) noexcept {
    Mat3 out;
    out.r[0][0] = 0.0;  out.r[0][1] = -a.z; out.r[0][2] =  a.y;
    out.r[1][0] =  a.z; out.r[1][1] = 0.0;  out.r[1][2] = -a.x;
    out.r[2][0] = -a.y; out.r[2][1] =  a.x; out.r[2][2] = 0.0;
    return out;
}

// --- Jacobian primitives, mechanically composed and each checked against a
// central finite difference before being trusted (own script, 5 random
// trials, ~1e-14 absolute agreement), not assumed from the calculus alone.
// `Mat3::r[i][j]` is d(component i)/d(input j) throughout, matching
// `dyn::StateJacobian`'s own convention (Drag::accel's own dadr).

/// d(v/|v|), given the ALREADY-normalized v_hat and |v|: the standard
/// unit-vector projector (I - v_hat v_hat^T)/|v|, left un-chained so a
/// caller composes it with v's own upstream Jacobian via `.times(...)`.
[[nodiscard]] Mat3 d_normalized(const Vec3& v_hat, double norm) noexcept {
    Mat3 proj;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            proj.r[i][j] = ((i == j) ? 1.0 : 0.0) - component(v_hat, i) * component(v_hat, j);
    return mat3_scale(1.0 / norm, proj);
}

/// d(a x b)/dx, given a(x), b(x) and their own d/dx -- either may be
/// `zero3()` when that side does not depend on x.
[[nodiscard]] Mat3 d_cross(const Vec3& a, const Mat3& da_dx, const Vec3& b, const Mat3& db_dx) noexcept {
    Mat3 out;
    for (std::size_t l = 0; l < 3; ++l)
        set_column(out, l, column(da_dx, l).cross(b) + a.cross(column(db_dx, l)));
    return out;
}

/// d(a.b)/dx, as a Vec3 standing in for a 1x3 row -- either da_dx/db_dx may
/// be `zero3()`.
[[nodiscard]] Vec3 d_dot(const Vec3& a, const Mat3& da_dx, const Vec3& b, const Mat3& db_dx) noexcept {
    double out[3];
    for (std::size_t l = 0; l < 3; ++l)
        out[l] = a.dot(column(db_dx, l)) + b.dot(column(da_dx, l));
    return Vec3{out[0], out[1], out[2]};
}

/// `ECOM-R-001`/`ECOM-R-002`'s own bundle: the Eq.1 frame, Delta-u, and
/// Delta-u's own analytic velocity gradient (a 1x3 row, as a Vec3).
struct Geometry {
    Vec3 e_D, e_Y, e_B;
    double delta_u = 0.0;
    Vec3 d_delta_u_d_v;
};

[[nodiscard]] odl::Result<Geometry, EcomError>
geometry_at(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs) {
    const Vec3 r_hat = normalized(r_gcrs_m);
    const Vec3 e_D = normalized(sun_direction_gcrs);  // ARN15 Eq.1: (r_Sun-r)/|r_Sun-r|, i.e. the
                                                       // satellite->Sun direction -- exactly the
                                                       // caller's own sun_direction_gcrs already.
    const Vec3 raw_Y = r_hat.cross(e_D);
    const double raw_Y_norm = raw_Y.norm();
    if (raw_Y_norm < kMinAxisNorm) {
        return odl::err(EcomError{"ECOM-F-001",
            "the spacecraft is within the nadir-Sun-line singularity's own tolerance: "
            "e_r x e_D has norm " + std::to_string(raw_Y_norm) + ", below " +
            std::to_string(kMinAxisNorm) + " -- undefined, not merely small"});
    }
    const Vec3 e_Y = (-1.0 / raw_Y_norm) * raw_Y;  // ARN15 Eq.1: -(e_r x e_D)/|e_r x e_D|
    const Vec3 e_B = e_D.cross(e_Y);

    const Vec3 n_raw = r_gcrs_m.cross(v_gcrs_m_per_s);
    const double n_raw_norm = n_raw.norm();
    const Vec3 n_hat = normalized(n_raw);
    const Vec3 t_hat = n_hat.cross(r_hat);

    const double sdotn = e_D.dot(n_hat);
    const Vec3 s_orb_raw = e_D - sdotn * n_hat;
    const double s_orb_norm = s_orb_raw.norm();
    if (s_orb_norm < kMinAxisNorm) {
        return odl::err(EcomError{"ECOM-F-002",
            "the Sun is within the orbit-normal singularity's own tolerance (beta ~ +-90 deg): "
            "the Sun's own projection onto the orbital plane has norm " +
            std::to_string(s_orb_norm) + ", below " + std::to_string(kMinAxisNorm) +
            " -- Delta-u is undefined, not merely small"});
    }
    const Vec3 s_orb_hat = (1.0 / s_orb_norm) * s_orb_raw;

    // Delta-u = angle FROM the Sun's own in-plane projection TO the
    // satellite, positive in the direction of motion (t_hat) -- ARN15's own
    // "independent of the coordinate system" formula taken directly, no
    // ascending-node reference (ECOM-A-002 checks the coordinate claim;
    // ECOM-A-004's own reduction test cross-checks this formula
    // independently, via a separately-built node construction).
    const double Y_ = -s_orb_hat.dot(t_hat);
    const double X_ = s_orb_hat.dot(r_hat);
    const double delta_u = std::atan2(Y_, X_);

    // d(Delta_u)/dv. r_hat and e_D are v-constant; every v-dependence enters
    // through n_hat = normalize(r x v) alone.
    const Mat3 Z3 = zero3();
    const Mat3 d_n_raw_dv = skew(r_gcrs_m);  // d(r x v)/dv, r fixed: (r x .) is linear in v
    const Mat3 d_n_hat_dv = d_normalized(n_hat, n_raw_norm).times(d_n_raw_dv);
    const Mat3 d_t_hat_dv = d_cross(n_hat, d_n_hat_dv, r_hat, Z3);  // r_hat v-constant

    const Vec3 d_sdotn_dv = d_dot(e_D, Z3, n_hat, d_n_hat_dv);  // e_D v-constant
    const Mat3 d_s_orb_raw_dv = mat3_scale(-1.0,
        mat3_add(outer(n_hat, d_sdotn_dv), mat3_scale(sdotn, d_n_hat_dv)));
    const Mat3 d_s_orb_hat_dv = d_normalized(s_orb_hat, s_orb_norm).times(d_s_orb_raw_dv);

    const Vec3 d_Y_dv = (-1.0) * d_dot(s_orb_hat, d_s_orb_hat_dv, t_hat, d_t_hat_dv);
    const Vec3 d_X_dv = d_dot(s_orb_hat, d_s_orb_hat_dv, r_hat, Z3);  // r_hat v-constant

    const double denom = X_ * X_ + Y_ * Y_;
    const Vec3 d_delta_u_dv = (1.0 / denom) * (X_ * d_Y_dv - Y_ * d_X_dv);

    return Geometry{e_D, e_Y, e_B, delta_u, d_delta_u_dv};
}

/// `ARN15` Eq.5's own D(Delta_u), B(Delta_u), and their own d/d(Delta_u) --
/// Y carries only Y0 (no du-dependence, ARN15's own printed form).
struct Harmonics {
    double D = 0.0, B = 0.0;
    double dD_ddu = 0.0, dB_ddu = 0.0;
};

[[nodiscard]] Harmonics evaluate(const EcomOrder& order, const EcomCoefficients& c, double du) noexcept {
    Harmonics h;
    h.D = c.D0;
    for (int i = 0; i < order.n_D; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const double n = 2.0 * static_cast<double>(i + 1);
        const double cs = std::cos(n * du), sn = std::sin(n * du);
        h.D += c.D_even_c[idx] * cs + c.D_even_s[idx] * sn;
        h.dD_ddu += n * (-c.D_even_c[idx] * sn + c.D_even_s[idx] * cs);
    }
    h.B = c.B0;
    for (int i = 0; i < order.n_B; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const double n = 2.0 * static_cast<double>(i + 1) - 1.0;
        const double cs = std::cos(n * du), sn = std::sin(n * du);
        h.B += c.B_odd_c[idx] * cs + c.B_odd_s[idx] * sn;
        h.dB_ddu += n * (-c.B_odd_c[idx] * sn + c.B_odd_s[idx] * cs);
    }
    return h;
}

/// Reads `order`-shaped coefficient values from `params`, walking
/// `ids.flattened()`'s own order -- the one place that order is assumed to
/// match `EcomCoefficients`' own field order (`ECOM-Q-001`).
[[nodiscard]] odl::Result<EcomCoefficients, dyn::DynError>
read_coefficients(const EcomOrder& order, const EcomParameterIds& ids, const dyn::ParameterSet& params) {
    EcomCoefficients c;
    c.D_even_c.resize(static_cast<std::size_t>(order.n_D));
    c.D_even_s.resize(static_cast<std::size_t>(order.n_D));
    c.B_odd_c.resize(static_cast<std::size_t>(order.n_B));
    c.B_odd_s.resize(static_cast<std::size_t>(order.n_B));

    auto read = [&](const dyn::ParameterId& id, double& slot) -> odl::Result<void, dyn::DynError> {
        auto v = params.value(id);
        if (!v.has_value()) return odl::err(v.error());
        slot = *v;
        return {};
    };
    if (auto r1 = read(ids.D0, c.D0); !r1.has_value()) return odl::err(r1.error());
    for (std::size_t i = 0; i < ids.D_even_c.size(); ++i) {
        if (auto r1 = read(ids.D_even_c[i], c.D_even_c[i]); !r1.has_value()) return odl::err(r1.error());
        if (auto r2 = read(ids.D_even_s[i], c.D_even_s[i]); !r2.has_value()) return odl::err(r2.error());
    }
    if (auto r1 = read(ids.Y0, c.Y0); !r1.has_value()) return odl::err(r1.error());
    if (auto r1 = read(ids.B0, c.B0); !r1.has_value()) return odl::err(r1.error());
    for (std::size_t i = 0; i < ids.B_odd_c.size(); ++i) {
        if (auto r1 = read(ids.B_odd_c[i], c.B_odd_c[i]); !r1.has_value()) return odl::err(r1.error());
        if (auto r2 = read(ids.B_odd_s[i], c.B_odd_s[i]); !r2.has_value()) return odl::err(r2.error());
    }
    return c;
}

/// `ds`'s own components, in exactly `EcomParameterIds::flattened()`'s own
/// order, so a caller can zip them with `flattened_ids_` for
/// `ParameterJacobian::set_column`.
[[nodiscard]] std::vector<Vec3> flatten_sensitivities(const EcomSensitivities& ds) {
    std::vector<Vec3> out;
    out.reserve(2 + 2 * ds.D_even_c.size() + 2 + 2 * ds.B_odd_c.size());
    out.push_back(ds.D0);
    for (std::size_t i = 0; i < ds.D_even_c.size(); ++i) {
        out.push_back(ds.D_even_c[i]);
        out.push_back(ds.D_even_s[i]);
    }
    out.push_back(ds.Y0);
    out.push_back(ds.B0);
    for (std::size_t i = 0; i < ds.B_odd_c.size(); ++i) {
        out.push_back(ds.B_odd_c[i]);
        out.push_back(ds.B_odd_s[i]);
    }
    return out;
}

}  // namespace

double d_of(const EcomOrder& order, const EcomCoefficients& c, double delta_u_rad) noexcept {
    return evaluate(order, c, delta_u_rad).D;
}
double b_of(const EcomOrder& order, const EcomCoefficients& c, double delta_u_rad) noexcept {
    return evaluate(order, c, delta_u_rad).B;
}

odl::Result<EcomResult, EcomError>
ecom_acceleration(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs,
                   const EcomOrder& order, const EcomCoefficients& coefficients) {
    auto geom = geometry_at(r_gcrs_m, v_gcrs_m_per_s, sun_direction_gcrs);
    if (!geom.has_value()) return odl::err(geom.error());

    const Harmonics h = evaluate(order, coefficients, geom->delta_u);
    const Vec3 accel = h.D * geom->e_D + coefficients.Y0 * geom->e_Y + h.B * geom->e_B;

    // d(accel)/d(each coefficient) -- exact and linear (Drag::accel's own
    // "d(a)/d(C_D) is exact and trivial" precedent, extended to nD/nB terms).
    EcomSensitivities ds;
    ds.D0 = geom->e_D;
    ds.D_even_c.resize(static_cast<std::size_t>(order.n_D));
    ds.D_even_s.resize(static_cast<std::size_t>(order.n_D));
    for (int i = 0; i < order.n_D; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const double n = 2.0 * static_cast<double>(i + 1);
        ds.D_even_c[idx] = std::cos(n * geom->delta_u) * geom->e_D;
        ds.D_even_s[idx] = std::sin(n * geom->delta_u) * geom->e_D;
    }
    ds.Y0 = geom->e_Y;
    ds.B0 = geom->e_B;
    ds.B_odd_c.resize(static_cast<std::size_t>(order.n_B));
    ds.B_odd_s.resize(static_cast<std::size_t>(order.n_B));
    for (int i = 0; i < order.n_B; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const double n = 2.0 * static_cast<double>(i + 1) - 1.0;
        ds.B_odd_c[idx] = std::cos(n * geom->delta_u) * geom->e_B;
        ds.B_odd_s[idx] = std::sin(n * geom->delta_u) * geom->e_B;
    }

    // d(accel)/dv = [dD/d(du) e_D + dB/d(du) e_B] (x) d(Delta_u)/dv -- e_D/
    // e_Y/e_B carry no v-dependence at all in this module's own Delta-u
    // construction (ECOM-R-002), so Delta-u is the only channel.
    const Vec3 dacc_ddu = h.dD_ddu * geom->e_D + h.dB_ddu * geom->e_B;
    const Mat3 d_velocity = outer(dacc_ddu, geom->d_delta_u_d_v);

    return EcomResult{accel, geom->e_D, geom->e_Y, geom->e_B, geom->delta_u, ds, d_velocity};
}

std::vector<dyn::ParameterId> EcomParameterIds::flattened() const {
    std::vector<dyn::ParameterId> out;
    out.reserve(2 + 2 * D_even_c.size() + 2 + 2 * B_odd_c.size());
    out.push_back(D0);
    for (std::size_t i = 0; i < D_even_c.size(); ++i) {
        out.push_back(D_even_c[i]);
        out.push_back(D_even_s[i]);
    }
    out.push_back(Y0);
    out.push_back(B0);
    for (std::size_t i = 0; i < B_odd_c.size(); ++i) {
        out.push_back(B_odd_c[i]);
        out.push_back(B_odd_s[i]);
    }
    return out;
}

Ecom::Ecom(EcomOrder order, EcomParameterIds param_ids, const eph::Ephemeris& ephemeris,
           odl::time::LeapTable leaps)
    : order_(order), param_ids_(std::move(param_ids)), flattened_ids_(param_ids_.flattened()),
      ephemeris_(ephemeris), leaps_(std::move(leaps)) {}

dyn::ForceId Ecom::id() const { return dyn::ForceId{"ecom"}; }

const std::vector<dyn::ParameterId>& Ecom::consumes() const { return flattened_ids_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Ecom::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
            const Vec3& v_m_per_s, const dyn::ParameterSet& params,
            const dyn::ParameterRegistry& registry) const {
    const Vec3 r = r_m.metres();

    auto sun_state = ephemeris_.geocentric_state(eph::Body::Sun, t, leaps_);
    if (!sun_state.has_value()) {
        return odl::err(dyn::DynError{"ECOM-F-004",
            "could not find the Sun's own GCRS direction: " + sun_state.error().message});
    }
    const Vec3 sun_position_m = 1000.0 * sun_state->position();  // ephemeris km -> m
    const Vec3 sun_direction = sun_position_m - r;

    auto live = read_coefficients(order_, param_ids_, params);
    if (!live.has_value()) return odl::err(live.error());

    auto centre = ecom_acceleration(r, v_m_per_s, sun_direction, order_, *live);
    if (!centre.has_value()) return odl::err(centre.error());

    // ECOM-R-006: d(a)/d(r) is not analytically tractable through this same
    // construction (n_hat's own r-dependence compounds through the D/Y/B
    // frame too), so it is taken by central difference here, in the
    // wrapper -- `Srp::accel`'s own established split (`PHPR-P-6`), not a
    // new choice.
    Mat3 d_position;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        Vec3 bump{};
        if (axis == 0) bump.x = kPositionStep_m;
        else if (axis == 1) bump.y = kPositionStep_m;
        else bump.z = kPositionStep_m;

        const Vec3 r_plus = r + bump;
        const Vec3 r_minus = r - bump;
        auto a_plus = ecom_acceleration(r_plus, v_m_per_s, sun_position_m - r_plus, order_, *live);
        auto a_minus = ecom_acceleration(r_minus, v_m_per_s, sun_position_m - r_minus, order_, *live);
        if (!a_plus.has_value()) return odl::err(a_plus.error());
        if (!a_minus.has_value()) return odl::err(a_minus.error());

        const Vec3 dcol = (1.0 / (2.0 * kPositionStep_m)) *
            (a_plus->acceleration_m_s2 - a_minus->acceleration_m_s2);
        d_position.r[0][axis] = dcol.x;
        d_position.r[1][axis] = dcol.y;
        d_position.r[2][axis] = dcol.z;
    }

    dyn::ParameterJacobian dparam(registry);
    const std::vector<Vec3> sens = flatten_sensitivities(centre->d_coefficients);
    for (std::size_t k = 0; k < flattened_ids_.size(); ++k) {
        auto r1 = dparam.set_column(flattened_ids_[k], sens[k]);
        if (!r1.has_value()) return odl::err(r1.error());
    }

    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{centre->acceleration_m_s2},
        dyn::StateJacobian::with_velocity(d_position, centre->d_velocity),
        std::move(dparam)};
}

}  // namespace odl::ecom
