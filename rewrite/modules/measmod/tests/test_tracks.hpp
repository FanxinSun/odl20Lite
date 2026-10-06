#pragma once
// test_tracks.hpp — closed-form stand-ins for the four abstractions (SPEC-measmod.md MEAS-R-064): a station and a target in uniform motion
// (the G3 tests), the drift family of the finite-difference gate (§6.2), a trajectory that refuses, and a constant Earth orientation.

#include <odl/core/units.hpp>
#include <odl/measmod/tracks.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odl::measmod::testing {

/// A station in uniform motion: s0 + v (t − t0), a fixed vertical, no vertical rate.
class UniformStation final : public StationTrack {
public:
    UniformStation(const odl::time::Epoch& t0, odl::Vec3 s0, odl::Vec3 v, odl::Vec3 up = odl::Vec3{0.0, 0.0, 1.0}) : t0_(t0), s0_(s0), v_(v), up_(up) {}
    [[nodiscard]] odl::Result<StationKinematics, MeasError> at(const odl::time::Epoch& when) const override {
        const double dt = when.difference(t0_).to_seconds();
        return StationKinematics{s0_ + dt * v_, v_, up_, odl::Vec3{}};
    }

private:
    odl::time::Epoch t0_;
    odl::Vec3 s0_, v_, up_;
};

/// A target in uniform motion: r0 + v (t − t0), metres, returned as the frames module's typed state (km).
class LinearTrajectory final : public Trajectory {
public:
    LinearTrajectory(const odl::time::Epoch& t0, odl::Vec3 r0, odl::Vec3 v) : t0_(t0), r0_(r0), v_(v) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const override {
        const double dt = when.difference(t0_).to_seconds();
        return odl::frames::GcrsState{when, odl::km_from_metres(r0_ + dt * v_), odl::km_from_metres(v_)};
    }

private:
    odl::time::Epoch t0_;
    odl::Vec3 r0_, v_;
};

/// The finite-difference gate's trajectory family (SPEC-measmod §6.2): the nominal trajectory r_nom(t) = r0 + v0 (t − t0) + ½ a0 (t − t0)² plus a
/// TIME-FIXED variation of the state at the epoch t_p by (δr, δv): r(t) = r_nom(t) + δr + δv (t − t_p). With δ = 0 it is the nominal trajectory;
/// the gate puts t_p at the nominal bounce (or emission) epoch, and the boundary test (MEAS-A-045) puts it at an epoch t₀ of its own.
class DriftTrajectory final : public Trajectory {
public:
    DriftTrajectory(const odl::time::Epoch& t0, odl::Vec3 r0, odl::Vec3 v0, odl::Vec3 a0, const odl::time::Epoch& t_p, odl::Vec3 dr = {}, odl::Vec3 dv = {})
        : t0_(t0), r0_(r0), v0_(v0), a0_(a0), tp_(t_p), dr_(dr), dv_(dv) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const override {
        const double dt = when.difference(t0_).to_seconds(), dtp = when.difference(tp_).to_seconds();
        const odl::Vec3 r = r0_ + dt * v0_ + (0.5 * dt * dt) * a0_ + dr_ + dtp * dv_;
        const odl::Vec3 v = v0_ + dt * a0_ + dv_;
        return odl::frames::GcrsState{when, odl::km_from_metres(r), odl::km_from_metres(v)};
    }

private:
    odl::time::Epoch t0_;
    odl::Vec3 r0_, v0_, a0_;
    odl::time::Epoch tp_;
    odl::Vec3 dr_, dv_;
};

/// A trajectory that answers up to `limit` seconds after its reference epoch and refuses beyond (a refusal with its own id).
class RefusingTrajectory final : public Trajectory {
public:
    RefusingTrajectory(const odl::time::Epoch& t0, odl::Vec3 r0, odl::Vec3 v, double limit_s) : inner_(t0, r0, v), t0_(t0), limit_s_(limit_s) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const override {
        if (when.difference(t0_).to_seconds() > limit_s_) return odl::err("TRAJ-F-999", "the test trajectory holds nothing this far past its reference epoch");
        return inner_.state_at(when);
    }

private:
    LinearTrajectory inner_;
    odl::time::Epoch t0_;
    double limit_s_;
};

/// A station track that refuses at an epoch more than `limit_s` after its reference epoch, with its own id.
class RefusingStation final : public StationTrack {
public:
    RefusingStation(const odl::time::Epoch& t0, odl::Vec3 s0, odl::Vec3 v, double limit_s) : inner_(t0, s0, v), t0_(t0), limit_s_(limit_s) {}
    [[nodiscard]] odl::Result<StationKinematics, MeasError> at(const odl::time::Epoch& when) const override {
        if (when.difference(t0_).to_seconds() > limit_s_) return odl::err("STN-F-999", "the test station holds nothing this far past its reference epoch");
        return inner_.at(when);
    }

private:
    UniformStation inner_;
    odl::time::Epoch t0_;
    double limit_s_;
};

/// A constant Earth orientation: a fixed rotation GCRS → ITRS and a fixed rotation vector.
class ConstantOrientation final : public EarthOrientation {
public:
    ConstantOrientation(odl::Mat3 gcrs_to_itrs, odl::Vec3 omega) : m_(gcrs_to_itrs), omega_(omega) {}
    [[nodiscard]] odl::Result<EarthOrientationSample, MeasError> at(const odl::time::Epoch&) const override { return EarthOrientationSample{m_, omega_}; }

private:
    odl::Mat3 m_;
    odl::Vec3 omega_;
};

/// A trajectory that answers with the recorded states at the recorded epochs, exactly, and refuses at any other epoch (a table is not an interpolator).
class TableTrajectory final : public Trajectory {
public:
    struct Row {
        odl::time::Epoch epoch;
        odl::Vec3 position_km, velocity_km_s;
    };
    explicit TableTrajectory(std::vector<Row> rows) : rows_(std::move(rows)) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const override {
        for (const Row& r : rows_)
            if (r.epoch == when) return odl::frames::GcrsState{when, r.position_km, r.velocity_km_s};
        return odl::err("TRAJ-F-998", "the test table holds no state at this epoch");
    }

private:
    std::vector<Row> rows_;
};

/// Two-body motion about the Earth from a state at an epoch, by the universal-variable form of Kepler's equation (closed form, no integrator, no J2): the stand-in a
/// test uses where the specification says "a two-body propagation from the nearest record" (SPEC-measmod MEAS-A-060).
class TwoBodyTrajectory final : public Trajectory {
public:
    TwoBodyTrajectory(const odl::time::Epoch& t0, odl::Vec3 r0_m, odl::Vec3 v0_m_s, double mu_m3_s2 = 3.986004415e14) : t0_(t0), r0_(r0_m), v0_(v0_m_s), mu_(mu_m3_s2) {}
    [[nodiscard]] odl::Result<odl::frames::GcrsState, MeasError> state_at(const odl::time::Epoch& when) const override {
        const double dt = when.difference(t0_).to_seconds();
        const double sqmu = std::sqrt(mu_), r0n = r0_.norm(), vr0 = r0_.dot(v0_) / r0n, alpha = 2.0 / r0n - v0_.dot(v0_) / mu_;   // alpha = 1/a
        auto stumpff = [](double z, double& c, double& s) {
            if (std::abs(z) < 1e-6) {
                c = 0.5 - z / 24.0 + z * z / 720.0;
                s = 1.0 / 6.0 - z / 120.0 + z * z / 5040.0;
            } else if (z > 0.0) {
                const double q = std::sqrt(z);
                c = (1.0 - std::cos(q)) / z;
                s = (q - std::sin(q)) / (q * q * q);
            } else {
                const double q = std::sqrt(-z);
                c = (std::cosh(q) - 1.0) / (-z);
                s = (std::sinh(q) - q) / (q * q * q);
            }
        };
        double chi = sqmu * std::abs(alpha) * dt, c = 0.0, s = 0.0;
        for (int it = 0; it < 100; ++it) {
            const double z = alpha * chi * chi;
            stumpff(z, c, s);
            const double f = r0n * vr0 / sqmu * chi * chi * c + (1.0 - alpha * r0n) * chi * chi * chi * s + r0n * chi - sqmu * dt;
            const double df = r0n * vr0 / sqmu * chi * (1.0 - z * s) + (1.0 - alpha * r0n) * chi * chi * c + r0n;
            const double step = f / df;
            chi -= step;
            if (std::abs(step) < 1e-10) break;
        }
        const double z = alpha * chi * chi;
        stumpff(z, c, s);
        const double fl = 1.0 - chi * chi / r0n * c, gl = dt - chi * chi * chi / sqmu * s;
        const odl::Vec3 r = fl * r0_ + gl * v0_;
        const double rn = r.norm();
        const double fd = sqmu / (rn * r0n) * (z * s - 1.0) * chi, gd = 1.0 - chi * chi / rn * c;
        const odl::Vec3 v = fd * r0_ + gd * v0_;
        return odl::frames::GcrsState{when, odl::km_from_metres(r), odl::km_from_metres(v)};
    }

private:
    odl::time::Epoch t0_;
    odl::Vec3 r0_, v0_;
    double mu_;
};

/// An Earth orientation that answers with the real chain's at an epoch `seconds` LATER than the one asked for: the rotation of a UT1 error of that size (the precession,
/// nutation and polar motion change by under 10^-9 rad in half a second). MEAS-A-060 (iv).
class OffsetOrientation final : public EarthOrientation {
public:
    OffsetOrientation(const EarthOrientation& inner, double seconds) : inner_(&inner), seconds_(seconds) {}
    [[nodiscard]] odl::Result<EarthOrientationSample, MeasError> at(const odl::time::Epoch& when) const override {
        return inner_->at(when.add(odl::time::Duration::from_seconds(seconds_)));
    }

private:
    const EarthOrientation* inner_;
    double seconds_;
};

/// The real chain's orientation at one reference epoch, carried to nearby epochs by an EXACT rigid rotation at the chain's own rate about its own axis: the matrix
/// carries arithmetic round-off only, never the double-precision limit of the real chain's Earth-rotation angle (SPEC-measmod §6.2, Amendment A1: the EXACT RIGID
/// ROTATION configuration of the gate, and the reference of the chain-floor assertion, MEAS-A-046). The reference sample is read once, at construction.
class RigidRotationOrientation final : public EarthOrientation {
public:
    RigidRotationOrientation(const EarthOrientation& real, const odl::time::Epoch& reference) : reference_(reference), m0_{}, omega_{} {
        auto s = real.at(reference);
        if (!s) throw std::runtime_error("RigidRotationOrientation: the reference orientation refused: " + std::string(s.error().id));
        m0_ = s->gcrs_to_itrs;
        omega_ = s->omega_gcrs_rad_s;
    }
    [[nodiscard]] odl::Result<EarthOrientationSample, MeasError> at(const odl::time::Epoch& when) const override {
        const double dt = when.difference(reference_).to_seconds();
        const double w = omega_.norm();
        const odl::Vec3 n = (1.0 / w) * omega_;
        const double angle = w * dt;
        const double sin_a = std::sin(angle);
        const double one_minus_cos = 2.0 * std::sin(0.5 * angle) * std::sin(0.5 * angle);   // 1 - cos a, without the cancellation
        // R(-a) about n: (cos a) I - (sin a) [n]x + (1 - cos a) n n^T
        const double nn[3] = {n.x, n.y, n.z};
        const double cross[3][3] = {{0.0, -nn[2], nn[1]}, {nn[2], 0.0, -nn[0]}, {-nn[1], nn[0], 0.0}};
        odl::Mat3 r{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) r.r[i][j] = one_minus_cos * nn[i] * nn[j] + (i == j ? 1.0 - one_minus_cos : 0.0) - sin_a * cross[i][j];
        return EarthOrientationSample{m0_.times(r), omega_};
    }

private:
    odl::time::Epoch reference_;
    odl::Mat3 m0_;
    odl::Vec3 omega_;
};

}  // namespace odl::measmod::testing
