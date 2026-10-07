#pragma once
// So that a failed comparison of a BigInt, a Rational or a Decimal shows the numbers: Catch2 finds these by argument-dependent lookup, which is why they live in the namespace of the types.

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/decimal.hpp>
#include <odl/devkit/rational.hpp>

#include <ostream>

namespace odl::devkit {

inline std::ostream& operator<<(std::ostream& os, const BigInt& v) { return os << v.to_decimal(); }
inline std::ostream& operator<<(std::ostream& os, const Rational& v) { return os << v.to_string(); }
inline std::ostream& operator<<(std::ostream& os, const Decimal& v) { return os << v.to_string(); }

}  // namespace odl::devkit
