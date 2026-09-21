#pragma once
// odl/macromodel/cited.hpp — a value that cannot exist without saying where it
// came from.
//
// SPEC-macromodel MCRM-R-004: "a value without a citation is a load error, not
// a warning."  A comment saying so is not enforcement; a constructor that
// refuses is.  `Cited<T>` has exactly one way to come into being --
// `cited(value, citation)` -- and it returns a Result, so the empty-citation
// case is a compile-time-visible branch a caller cannot silently drop
// (`odl::Result`'s own [[nodiscard]], plan §5 constraint 8).

#include <odl/core/result.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace odl::macromodel {

using MacromodelError = odl::Diagnostic;

/// `T` together with the citation that justifies it.  Opaque: the only way to
/// get one is `cited()` below, which is the enforcement point.
template <class T>
class Cited {
public:
    [[nodiscard]] const T& value() const noexcept { return value_; }
    [[nodiscard]] const std::string& citation() const noexcept { return citation_; }

private:
    template <class U>
    friend odl::Result<Cited<U>, MacromodelError> cited(U value, std::string citation);

    Cited(T value, std::string citation) : value_(std::move(value)), citation_(std::move(citation)) {}

    T value_;
    std::string citation_;
};

/// Whitespace-only counts as empty: a citation of `" "` would pass a bare
/// `.empty()` check and read, to the next person, as cited.  MCRM-F-001.
[[nodiscard]] inline bool is_blank(std::string_view s) noexcept {
    for (char c : s)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
    return true;
}

/// The one constructor. MCRM-F-001: refuses a blank citation rather than
/// accepting a placeholder, so that "cited" in the type's name is never a
/// claim this function did not check.
template <class T>
[[nodiscard]] odl::Result<Cited<T>, MacromodelError> cited(T value, std::string citation) {
    if (is_blank(citation))
        return odl::err(MacromodelError{"MCRM-F-001", "a value was given an empty or "
                         "whitespace-only citation; every stored value must name where it "
                         "came from"});
    return Cited<T>(std::move(value), std::move(citation));
}

}  // namespace odl::macromodel
