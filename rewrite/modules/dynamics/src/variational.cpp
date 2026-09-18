// variational.cpp — SPEC-stm.md.

#include <odl/dynamics/variational.hpp>

#include <odl/core/units.hpp>

#include <cmath>

namespace odl::dyn {
namespace {

using odl::integrators::Vec;

constexpr std::size_t kN = 43;   // 6 state + 36 Phi + the integral of tr(A)

/// The 42-vector's layout, named once so that no index appears twice.
constexpr std::size_t kPhi0 = 6;
constexpr std::size_t phi_at(std::size_t i, std::size_t j) noexcept { return kPhi0 + i * 6 + j; }
constexpr std::size_t kTrace = 42;

struct Context {
    const ForceSet* forces;
    const ParameterSet* params;
    odl::time::Epoch t0;
    mutable double neglected = 0.0;
    mutable bool failed = false;
    mutable DynError error{"DYN-F-006", "no fault"};
};

}  // namespace

odl::Result<StmSolution, DynError>
propagate_with_stm(const ForceSet& forces, const ParameterSet& params,
                   const frames::State<frames::Frame::GCRS>& x0, double seconds,
                   double tolerance, odl::integrators::Control ctl) {
    Context ctx{&forces, &params, x0.epoch()};

    Vec<kN> y{};
    y[0] = x0.position().x; y[1] = x0.position().y; y[2] = x0.position().z;
    y[3] = x0.velocity().x; y[4] = x0.velocity().y; y[5] = x0.velocity().z;
    for (std::size_t i = 0; i < 6; ++i) y[phi_at(i, i)] = 1.0;

    auto rhs = [&ctx](double s, const Vec<kN>& u) -> Vec<kN> {
        Vec<kN> d{};
        const auto when = ctx.t0.add(odl::time::Duration::from_seconds(s));
        const frames::State<frames::Frame::GCRS> st{when, Vec3{u[0], u[1], u[2]},
                                                          Vec3{u[3], u[4], u[5]}};
        auto der = ctx.forces->derivative(when, st, *ctx.params);
        if (!der) {
            if (!ctx.failed) { ctx.failed = true; ctx.error = der.error(); }
            return d;   // the integrator's finite check will stop it
        }
        auto jac = ctx.forces->jacobians(when, st, *ctx.params);
        if (!jac) {
            if (!ctx.failed) { ctx.failed = true; ctx.error = jac.error(); }
            return d;
        }
        ctx.neglected = jac->neglected_velocity_bound_per_s;

        d[0] = der->velocity_km_s.x; d[1] = der->velocity_km_s.y; d[2] = der->velocity_km_s.z;
        d[3] = der->acceleration_km_s2.x;
        d[4] = der->acceleration_km_s2.y;
        d[5] = der->acceleration_km_s2.z;

        // dPhi/dt = A Phi, with A's blocks in 1/s^2 and 1/s -- unit-invariant
        // under the km/m scaling, so nothing is converted here.
        for (std::size_t j = 0; j < 6; ++j) {
            for (std::size_t i = 0; i < 3; ++i) d[phi_at(i, j)] = u[phi_at(i + 3, j)];
            for (std::size_t i = 0; i < 3; ++i) {
                double acc = 0.0;
                for (std::size_t k = 0; k < 3; ++k)
                    acc += jac->da_dr.r[i][k] * u[phi_at(k, j)]
                         + jac->da_dv.r[i][k] * u[phi_at(k + 3, j)];
                d[phi_at(i + 3, j)] = acc;
            }
        }
        // Liouville's integrand. tr(A) = tr(0) + tr(da/dv) = tr(da/dv), since A's
        // top-left block is zero and its top-right is the identity, which sits
        // off the diagonal of the 6x6.
        d[kTrace] = jac->da_dv.r[0][0] + jac->da_dv.r[1][1] + jac->da_dv.r[2][2];
        return d;
    };

    auto sol = odl::integrators::integrate<Vec<kN>>(rhs, 0.0, y, seconds, tolerance, ctl);
    if (ctx.failed) return odl::err(ctx.error);
    if (!sol) {
        return odl::err(DynError{sol.error().id, sol.error().message});
    }

    StmSolution out{frames::State<frames::Frame::GCRS>{
                        x0.epoch().add(odl::time::Duration::from_seconds(seconds)),
                        Vec3{sol->y[0], sol->y[1], sol->y[2]},
                        Vec3{sol->y[3], sol->y[4], sol->y[5]}},
                    Mat6{}, sol->record, sol->y[kTrace], ctx.neglected};
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) out.phi[i][j] = sol->y[phi_at(i, j)];
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j)
            if (!std::isfinite(out.phi[i][j]))
                return odl::err(DynError{"STM-F-002",
                    "Phi(" + std::to_string(i) + "," + std::to_string(j) + ") is not finite"});
    return out;
}

odl::Result<frames::State<frames::Frame::GCRS>, DynError>
propagate(const ForceSet& forces, const ParameterSet& params,
          const frames::State<frames::Frame::GCRS>& x0, double seconds, double tolerance,
          odl::integrators::Control ctl) {
    auto r = propagate_with_stm(forces, params, x0, seconds, tolerance, ctl);
    if (!r) return odl::err(r.error());
    return r->state;
}

}  // namespace odl::dyn

namespace odl::dyn {

odl::Result<std::array<double, 6>, DynError>
SensitivitySolution::column(const ParameterRegistry& reg, const ParameterId& id) const {
    // Addressed by IDENTITY. There is no column(std::size_t) and adding one
    // would be the defect DYN-R-001 exists to prevent.
    if (!reg.issued(id)) {
        return odl::err(DynError{"DYN-F-003",
            "this ParameterId was not issued by the registry this solution was built for; its "
            "width is " + std::to_string(width) + "."});
    }
    auto decl = reg.declaration(id);
    if (!decl) return odl::err(decl.error());
    const std::size_t slot = id.slot_;   // private; this type is a friend
    std::array<double, 6> out{};
    for (std::size_t i = 0; i < 6; ++i) out[i] = sensitivities[i * width + slot];
    return out;
}

odl::Result<SensitivitySolution, DynError>
propagate_with_sensitivities(const ForceSet& forces, const ParameterSet& params,
                             const ParameterRegistry& registry,
                             const frames::State<frames::Frame::GCRS>& x0, double seconds,
                             double tolerance, odl::integrators::Control ctl) {
    const std::size_t n = registry.size();
    const std::size_t width = 43 + 6 * n;        // state, Phi, trace, then S
    Context ctx{&forces, &params, x0.epoch()};

    odl::integrators::DynVec y(width, 0.0);
    y[0] = x0.position().x; y[1] = x0.position().y; y[2] = x0.position().z;
    y[3] = x0.velocity().x; y[4] = x0.velocity().y; y[5] = x0.velocity().z;
    for (std::size_t i = 0; i < 6; ++i) y[phi_at(i, i)] = 1.0;
    // S(t0) = 0: the initial state does not depend on the parameters.

    auto rhs = [&ctx, &registry, n](double s, const odl::integrators::DynVec& u) {
        odl::integrators::DynVec d(u.size(), 0.0);
        const auto when = ctx.t0.add(odl::time::Duration::from_seconds(s));
        const frames::State<frames::Frame::GCRS> st{when, Vec3{u[0], u[1], u[2]},
                                                          Vec3{u[3], u[4], u[5]}};
        auto der = ctx.forces->derivative(when, st, *ctx.params);
        if (!der) { if (!ctx.failed) { ctx.failed = true; ctx.error = der.error(); } return d; }
        auto jac = ctx.forces->jacobians(when, st, *ctx.params);
        if (!jac) { if (!ctx.failed) { ctx.failed = true; ctx.error = jac.error(); } return d; }
        ctx.neglected = jac->neglected_velocity_bound_per_s;

        d[0] = der->velocity_km_s.x; d[1] = der->velocity_km_s.y; d[2] = der->velocity_km_s.z;
        d[3] = der->acceleration_km_s2.x;
        d[4] = der->acceleration_km_s2.y;
        d[5] = der->acceleration_km_s2.z;
        for (std::size_t j = 0; j < 6; ++j) {
            for (std::size_t i = 0; i < 3; ++i) d[phi_at(i, j)] = u[phi_at(i + 3, j)];
            for (std::size_t i = 0; i < 3; ++i) {
                double acc = 0.0;
                for (std::size_t k = 0; k < 3; ++k)
                    acc += jac->da_dr.r[i][k] * u[phi_at(k, j)]
                         + jac->da_dv.r[i][k] * u[phi_at(k + 3, j)];
                d[phi_at(i + 3, j)] = acc;
            }
        }
        d[kTrace] = jac->da_dv.r[0][0] + jac->da_dv.r[1][1] + jac->da_dv.r[2][2];

        // dS/dt = A S + B. The loop's bound is n, which came from the registry;
        // nothing here is a compile-time extent.
        const std::size_t S0 = 43;
        // B = [0; da/dp]. da/dp is in m/s^2 per unit parameter and the state
        // derivative is in km/s^2, so this term crosses -- ONCE, by name, and
        // `ci.sh` gate 11's register is what keeps it the only one here.
        const auto& dp = jac->da_dp.dense();
        std::vector<std::array<double, 3>> bx(n);
        for (std::size_t c = 0; c < n; ++c) {
            const Vec3 k = state_accel_km_s2_from_m_s2(dp[c]);
            bx[c] = {k.x, k.y, k.z};
        }
        for (std::size_t c = 0; c < n; ++c) {
            for (std::size_t i = 0; i < 3; ++i) d[S0 + i * n + c] = u[S0 + (i + 3) * n + c];
            for (std::size_t i = 0; i < 3; ++i) {
                double acc = 0.0;
                for (std::size_t k = 0; k < 3; ++k)
                    acc += jac->da_dr.r[i][k] * u[S0 + k * n + c]
                         + jac->da_dv.r[i][k] * u[S0 + (k + 3) * n + c];
                d[S0 + (i + 3) * n + c] = acc + bx[c][i];
            }
        }
        return d;
    };

    auto sol = odl::integrators::integrate<odl::integrators::DynVec>(
        rhs, 0.0, y, seconds, tolerance, ctl);
    if (ctx.failed) return odl::err(ctx.error);
    if (!sol) return odl::err(DynError{sol.error().id, sol.error().message});

    SensitivitySolution out{
        frames::State<frames::Frame::GCRS>{
            x0.epoch().add(odl::time::Duration::from_seconds(seconds)),
            Vec3{sol->y[0], sol->y[1], sol->y[2]}, Vec3{sol->y[3], sol->y[4], sol->y[5]}},
        Mat6{}, std::vector<double>(6 * n, 0.0), n, sol->record, sol->y[kTrace], ctx.neglected};
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) out.phi[i][j] = sol->y[phi_at(i, j)];
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t c = 0; c < n; ++c) out.sensitivities[i * n + c] = sol->y[43 + i * n + c];
    return out;
}

}  // namespace odl::dyn
