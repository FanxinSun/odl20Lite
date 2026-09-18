#pragma once
// odl/core/result.hpp — the fallible-return contract for the whole tree.
//
// Decision D7: the language is C++20 (D1) and std::expected is C++23, so
// tl::expected is vendored through the manifest like any other input and
// aliased here.  D7 was taken on reversibility: C++20 -> C++23 later is two
// CMake lines and deleting this shim, where the reverse would mean hunting
// every C++23 feature that had crept in over months.
//
// PLAN §5 CONSTRAINT 8 LIVES HERE.  That one-line migration holds only while
// the tree stays inside the part of the API where tl::expected and
// std::expected are interchangeable — construction, checking, unwrapping.  The
// monadic operations are where they diverge.  So:
//
//     * every signature names odl::Result, never tl::expected;
//     * no and_then, or_else, transform or transform_error, ever.
//
// tools/constraint8.py enforces both and CI runs it, because a constraint that
// depends on everyone remembering it is a constraint that expires quietly.
//
// WHY A RETURN VALUE AND NOT AN EXCEPTION.  SPEC-template.md §5 R-ERR-1: a
// diagnostic is a returned value, and there is no module-level accumulator that
// could carry one operation's failure into the next.  Plan §5 constraint 4
// requires a refusal rather than an approximation, and a refusal that unwinds
// is not a diagnostic the caller must consume — it is one a bare catch can
// discard.  [[nodiscard]] plus -Werror=unused-result is what makes "must
// consume" mean something.

#include <tl/expected.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace odl {

/// A refusal.  Carries the specification's own refusal identifier so that a
/// diagnostic can be traced to the clause that required it, and a message that
/// must name the offending values — "out of range" is not a diagnostic;
/// "requested 2031-04-02T00:00:00 UTC (MJD 62867.0); table covers ..." is.
struct Diagnostic {
    std::string_view id;   ///< e.g. "TIME-F-002"; a static literal from the spec
    std::string      message;

    Diagnostic(std::string_view refusal_id, std::string text)
        : id(refusal_id), message(std::move(text)) {}
};

/// The fallible return type.  T or a diagnostic, and the caller cannot ignore
/// which without the compiler saying so.
template <class T, class E = Diagnostic>
using Result = tl::expected<T, E>;

/// Construct the error alternative.  `return odl::err(...)` reads as the
/// refusal it is.
template <class E>
[[nodiscard]] constexpr auto err(E e) -> tl::unexpected<E> {
    return tl::unexpected<E>(std::move(e));
}

/// The common case: a refusal naming its specification clause.
[[nodiscard]] inline auto err(std::string_view refusal_id, std::string message)
    -> tl::unexpected<Diagnostic> {
    return tl::unexpected<Diagnostic>(Diagnostic{refusal_id, std::move(message)});
}

}  // namespace odl
