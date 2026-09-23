#pragma once
// odl/macromodel/macromodel.hpp — N surfaces, two kinds, every value cited.
//
// SPEC-macromodel.md.  DESIGNED FOR THE GENERAL CASE, GATED ON A CANNONBALL --
// the plan's own words for this step, and what that means precisely is in
// MCRM-R-001..003's header comments here, next to the code that makes it true:
//
//   any number of surfaces, EITHER a FlatSurface (a normal, either body-fixed
//   or sun-tracking) OR a SphericalSurface (no normal at all -- a sphere
//   presents the same silhouette from every direction, so giving it one would
//   misstate the physics), freely mixed;
//
//   every area, every optical coefficient, the mass, and the centre of mass is
//   a Cited<T> -- MCRM-R-004 -- so an uncited value is not a value this type
//   can hold, not a rule this type documents.
//
// A reader who needs a third surface kind must change this file and will see
// which choice they are changing.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace odl::macromodel {

/// How a FlatSurface's normal is determined.  SunPointing carries NO payload,
/// on purpose (SPEC-macromodel §3): a sun-tracking solar panel's normal IS the
/// Sun direction, by the tracking law that points it there, not a separate
/// value that happens to usually agree with the Sun direction.  Storing a
/// body-fixed vector for it would be a second, competing definition of the
/// same quantity -- the fault SHDW-R-029's secular-pole rule names, here for a
/// different pair of quantities.
enum class NormalMode { body_fixed, sun_pointing };

/// PHPR-R-004a/R-004b.  A surface's optical triple, all three or none: a plain
/// `std::optional<Cited<double>>` per field would let a caller supply two of
/// three and leave the kernel to guess the third, the same disagreement
/// `MCRM-R-002` already refuses for `normal_mode`/`body_fixed_normal` -- one
/// struct, so "all or nothing" is a type the compiler enforces rather than an
/// invariant documented and trusted.
struct OpticalTriple {
    Cited<double> absorptivity;
    Cited<double> specular;
    Cited<double> diffuse;
};

/// Which spectral band a triple describes. RS09 Table 3.1 prints both for
/// GPS panels and states plainly that the materials behave differently in
/// each (front, visible: mu 0.85, nu 0.23; front, infrared: mu 0.50,
/// nu 0.20) -- not a refinement, a different value already measured for a
/// real spacecraft.
enum class Band { visible, infrared };

/// PHPR-R-004b.  A face's own optics: visible required (what this type held
/// before this field existed), infrared optional. Absent infrared falls back
/// to the visible triple -- a stated approximation (`SPEC-photon-pressure`
/// §4.4: SRP and albedo are visible-band, the Earth's own emission is
/// infrared, and using one triple for both is exactly the "front is 0.85
/// where the source says 0.50" error a caller must not fall into silently).
struct BandedOptics {
    OpticalTriple visible;
    std::optional<OpticalTriple> infrared;

    [[nodiscard]] const OpticalTriple& in(Band b) const noexcept {
        return (b == Band::infrared && infrared.has_value()) ? *infrared : visible;
    }
};

/// MCRM-R-002.  Immutable after construction, and constructible only through
/// `flat_surface_body_fixed` / `flat_surface_sun_pointing` below: a plain
/// aggregate pairing `normal_mode` with an independent
/// `optional<BodyDirection>` field would let the two disagree (body_fixed mode
/// with no normal supplied, or sun_pointing mode carrying a normal that would
/// then be silently ignored) -- a mismatch nothing would catch until
/// `srp_force` dereferenced an empty optional. Two named factories, one per
/// mode, make the pairing a constructor-time fact rather than an invariant a
/// reader has to trust.
class FlatSurface {
public:
    [[nodiscard]] const Cited<double>& area_m2() const noexcept { return area_m2_; }
    [[nodiscard]] NormalMode normal_mode() const noexcept { return normal_mode_; }
    /// Populated iff normal_mode() == body_fixed; the two factories below are
    /// the only constructors and each sets exactly the field its mode needs.
    [[nodiscard]] const std::optional<BodyDirection>& body_fixed_normal() const noexcept {
        return body_fixed_normal_;
    }
    [[nodiscard]] const Cited<double>& absorptivity() const noexcept { return front_.visible.absorptivity; }
    [[nodiscard]] const Cited<double>& specular() const noexcept { return front_.visible.specular; }
    [[nodiscard]] const Cited<double>& diffuse() const noexcept { return front_.visible.diffuse; }
    /// PHPR-R-004b.  The front face's optics, resolved for the requested band
    /// (infrared falling back to visible if not separately stated).  What
    /// `photon_force` actually calls -- the three accessors above exist only
    /// so every pre-existing (front, visible) caller compiles unchanged.
    [[nodiscard]] const OpticalTriple& front(Band b) const noexcept { return front_.in(b); }
    /// PHPR-R-004a/R-004b.  Absent: one-sided, as every surface was before
    /// this field existed -- a bus face whose back is inside the body,
    /// unchanged.  Present: `photon_force` evaluates this surface's back,
    /// with the REVERSED normal and these properties (resolved for the
    /// requested band the same way the front is), whenever the illumination
    /// source is behind the front (`cos(theta) < 0` on the front normal).
    [[nodiscard]] std::optional<OpticalTriple> back(Band b) const noexcept {
        if (!back_.has_value()) return std::nullopt;
        return back_->in(b);
    }

private:
    friend odl::Result<FlatSurface, MacromodelError>
    flat_surface_body_fixed(Cited<double> area_m2, BodyDirection normal, Cited<double> absorptivity,
                            Cited<double> specular, Cited<double> diffuse,
                            std::optional<OpticalTriple> front_infrared,
                            std::optional<BandedOptics> back);
    friend odl::Result<FlatSurface, MacromodelError>
    flat_surface_sun_pointing(Cited<double> area_m2, Cited<double> absorptivity,
                              Cited<double> specular, Cited<double> diffuse,
                              std::optional<OpticalTriple> front_infrared,
                              std::optional<BandedOptics> back);

    FlatSurface(Cited<double> area_m2, NormalMode mode, std::optional<BodyDirection> normal,
               BandedOptics front, std::optional<BandedOptics> back)
        : area_m2_(std::move(area_m2)), normal_mode_(mode), body_fixed_normal_(std::move(normal)),
          front_(std::move(front)), back_(std::move(back)) {}

    Cited<double> area_m2_;
    NormalMode normal_mode_;
    std::optional<BodyDirection> body_fixed_normal_;
    BandedOptics front_;                  ///< alpha/rho/delta, RHS12's own symbols; PHPR-R-004b
    std::optional<BandedOptics> back_;    ///< PHPR-R-004a/R-004b
};

/// A surface whose normal is a body-fixed constant (a bus panel).
/// `front_infrared`/`back` default to absent (PHPR-R-004a/R-004b), so every
/// pre-existing call site compiles unchanged: one-sided, visible-band-only,
/// exactly as before these fields existed.
[[nodiscard]] inline odl::Result<FlatSurface, MacromodelError>
flat_surface_body_fixed(Cited<double> area_m2, BodyDirection normal, Cited<double> absorptivity,
                        Cited<double> specular, Cited<double> diffuse,
                        std::optional<OpticalTriple> front_infrared = std::nullopt,
                        std::optional<BandedOptics> back = std::nullopt) {
    BandedOptics front{OpticalTriple{std::move(absorptivity), std::move(specular), std::move(diffuse)},
                       std::move(front_infrared)};
    return FlatSurface(std::move(area_m2), NormalMode::body_fixed, normal, std::move(front),
                       std::move(back));
}

/// A surface whose normal is defined to equal the Sun direction at every
/// evaluation (a sun-tracking solar panel) -- SPEC-macromodel §3.  Takes no
/// normal at all: there is nothing to disagree with the tracking law.
/// `back` defaults to absent (PHPR-R-004a); a sun-tracking panel's own back
/// is exactly the case this field exists for -- the Sun never lights it, but
/// a different body's radiation (ERP's Earth) can.
[[nodiscard]] inline odl::Result<FlatSurface, MacromodelError>
flat_surface_sun_pointing(Cited<double> area_m2, Cited<double> absorptivity,
                          Cited<double> specular, Cited<double> diffuse,
                          std::optional<OpticalTriple> front_infrared = std::nullopt,
                          std::optional<BandedOptics> back = std::nullopt) {
    BandedOptics front{OpticalTriple{std::move(absorptivity), std::move(specular), std::move(diffuse)},
                       std::move(front_infrared)};
    return FlatSurface(std::move(area_m2), NormalMode::sun_pointing, std::nullopt, std::move(front),
                       std::move(back));
}

/// MCRM-R-003.  No normal field, deliberately -- see this file's header.
/// `infrared` is trailing and optional (PHPR-R-004b) so every pre-existing
/// aggregate-initialised `SphericalSurface{area, a, s, d}` still compiles: a
/// C++ aggregate leaves trailing members it was not given value-initialised,
/// `std::nullopt` for an `optional`.  Absent, `in(Band)` falls back to the
/// visible triple exactly as `BandedOptics` does.
struct SphericalSurface {
    Cited<double> cross_section_area_m2;   ///< pi * r^2; the area a beam sees
    Cited<double> absorptivity;
    Cited<double> specular;    ///< stored for completeness; MCRM-R-007 proves it
                                ///< does not affect the net force
    Cited<double> diffuse;
    std::optional<OpticalTriple> infrared;

    [[nodiscard]] OpticalTriple in(Band b) const {
        if (b == Band::infrared && infrared.has_value()) return *infrared;
        return OpticalTriple{absorptivity, specular, diffuse};
    }
};

using Surface = std::variant<FlatSurface, SphericalSurface>;

/// MCRM-R-001.  Immutable after construction; built only through
/// MacromodelBuilder below, which is the enforcement point for MCRM-F-003 --
/// the two fields below are the only ones a caller sets INCREMENTALLY (a
/// surface's own fields are already fully cited the moment it is constructed,
/// since FlatSurface/SphericalSurface hold nothing but Cited<T> and
/// std::optional<BodyDirection>).
class Macromodel {
public:
    [[nodiscard]] const std::vector<Surface>& surfaces() const noexcept { return surfaces_; }
    [[nodiscard]] const Cited<double>& mass_kg() const noexcept { return mass_kg_; }
    [[nodiscard]] const Cited<Vec3>& centre_of_mass_m() const noexcept { return com_m_; }

private:
    friend class MacromodelBuilder;
    Macromodel(std::vector<Surface> surfaces, Cited<double> mass_kg, Cited<Vec3> com_m)
        : surfaces_(std::move(surfaces)), mass_kg_(std::move(mass_kg)), com_m_(std::move(com_m)) {}

    std::vector<Surface> surfaces_;
    Cited<double> mass_kg_;
    Cited<Vec3> com_m_;
};

/// Accumulates surfaces, then requires mass and centre of mass to be set
/// before `build()` succeeds -- MCRM-F-003.  A surface's own area/optical
/// fields cannot reach this class uncited at all (they are Cited<T> already by
/// the time `add_surface` is called), so this builder's own refusal surface is
/// exactly the two macromodel-level fields nothing else enforces.
class MacromodelBuilder {
public:
    MacromodelBuilder& add_surface(Surface s) {
        surfaces_.push_back(std::move(s));
        return *this;
    }
    MacromodelBuilder& set_mass(Cited<double> mass_kg) {
        mass_kg_ = std::move(mass_kg);
        return *this;
    }
    MacromodelBuilder& set_centre_of_mass(Cited<Vec3> com_m) {
        com_m_ = std::move(com_m);
        return *this;
    }

    [[nodiscard]] odl::Result<Macromodel, MacromodelError> build() && {
        if (!mass_kg_.has_value())
            return odl::err(MacromodelError{"MCRM-F-003", "the macromodel's mass was never set"});
        if (!com_m_.has_value())
            return odl::err(MacromodelError{"MCRM-F-003",
                             "the macromodel's centre of mass was never set"});
        return Macromodel(std::move(surfaces_), std::move(*mass_kg_), std::move(*com_m_));
    }

private:
    std::vector<Surface> surfaces_;
    std::optional<Cited<double>> mass_kg_;
    std::optional<Cited<Vec3>> com_m_;
};

}  // namespace odl::macromodel
