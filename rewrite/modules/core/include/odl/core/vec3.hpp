#pragma once
// odl/core/vec3.hpp — three-vectors and 3x3 rotations.
//
// Deliberately small.  This tree needs the operations a rotation chain uses and
// nothing else; a general linear-algebra dependency would be a large surface
// acquired for a handful of operations, and every one of them would then need
// pinning, licence-checking and a NOTICE entry.

#include <array>
#include <cmath>

namespace odl {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    [[nodiscard]] double norm() const noexcept { return std::sqrt(x * x + y * y + z * z); }
    [[nodiscard]] double dot(const Vec3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vec3 cross(const Vec3& o) const noexcept {
        return Vec3{y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    friend Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Vec3 operator*(double s, Vec3 a) noexcept { return {s * a.x, s * a.y, s * a.z}; }
    friend Vec3 operator*(Vec3 a, double s) noexcept { return s * a; }
};

/// Row-major.  A matrix named M_a_to_b maps components in a to components in b.
struct Mat3 {
    std::array<std::array<double, 3>, 3> r{};

    static Mat3 identity() noexcept {
        Mat3 m; m.r[0][0] = m.r[1][1] = m.r[2][2] = 1.0; return m;
    }
    [[nodiscard]] Vec3 apply(const Vec3& v) const noexcept {
        return Vec3{r[0][0] * v.x + r[0][1] * v.y + r[0][2] * v.z,
                    r[1][0] * v.x + r[1][1] * v.y + r[1][2] * v.z,
                    r[2][0] * v.x + r[2][1] * v.y + r[2][2] * v.z};
    }
    /// The inverse of a rotation, and the ONLY inverse this tree uses.
    /// SPEC-frames FRAME-R-004: never numerical inversion. Transposing is exact,
    /// so the round trip closes to rounding rather than to a solver's tolerance.
    [[nodiscard]] Mat3 transpose() const noexcept {
        Mat3 t;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) t.r[i][j] = r[j][i];
        return t;
    }
    [[nodiscard]] Mat3 times(const Mat3& o) const noexcept {
        Mat3 p;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < 3; ++k) s += r[i][k] * o.r[k][j];
                p.r[i][j] = s;
            }
        return p;
    }
    [[nodiscard]] double determinant() const noexcept {
        return r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) -
               r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) +
               r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
    }
};

}  // namespace odl
