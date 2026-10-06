#pragma once
// odl/devkit/bytes.hpp — the byte-buffer vocabulary of the development tools.
//
// The devkit is the support library of tools/ (L0 step 8, plan §5 constraint 11): it links NOTHING of the tree's modules, so that a generator
// can never share code with the thing it checks.  What the tools used to get from Python's standard library — SHA-256, JSON, inflate, tar and
// zip, exact arithmetic, process spawning — is here, written for this tree and tested against its own committed bytes.

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace odl::devkit {

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

[[nodiscard]] inline ByteView as_bytes(std::string_view s) noexcept {
    return ByteView{reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

[[nodiscard]] inline std::string_view as_text(ByteView b) noexcept {
    return std::string_view{reinterpret_cast<const char*>(b.data()), b.size()};
}

}  // namespace odl::devkit
