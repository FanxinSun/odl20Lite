#pragma once
// odl/macromodel/irradiance.hpp — an irradiance, with its unit in its type.
//
// PHPR-R-001 (SPEC-photon-pressure §3, plan §5 constraint 10): the photon-
// pressure kernel used to take a file-local constant, 1367 W/m^2 at 1 au, with
// no way for a caller to supply anything else. A caller who wants a bare
// double has to say so by calling `.watts_per_m2()`, the same friction
// `BodyDirection::vec()` already imposes and for the same reason -- a value
// that carries its unit cannot silently become a value that does not.

#include <odl/core/result.hpp>
#include <odl/macromodel/cited.hpp>

#include <cmath>
#include <string>

namespace odl::macromodel {

/// Opaque: `irradiance_w_per_m2()` is the only way in.
class IrradianceWPerM2 {
public:
    [[nodiscard]] double watts_per_m2() const noexcept { return v_; }

private:
    friend odl::Result<IrradianceWPerM2, MacromodelError> irradiance_w_per_m2(double v);

    explicit IrradianceWPerM2(double v) noexcept : v_(v) {}
    double v_;
};

/// MCRM-F-006.  Irradiance is a physical flux density: negative, infinite or
/// NaN is not a smaller or larger irradiance, it is not one at all.
[[nodiscard]] inline odl::Result<IrradianceWPerM2, MacromodelError> irradiance_w_per_m2(double v) {
    if (!std::isfinite(v) || v < 0.0) {
        return odl::err(MacromodelError{"MCRM-F-006",
            "an irradiance was not a finite, non-negative flux density: " + std::to_string(v) +
            " W/m^2"});
    }
    return IrradianceWPerM2(v);
}

}  // namespace odl::macromodel
