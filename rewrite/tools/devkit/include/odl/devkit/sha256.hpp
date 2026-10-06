#pragma once
// odl/devkit/sha256.hpp — SHA-256 (FIPS 180-4), the identity of every input this tree declares.
//
// Written here, not taken from a library, because the fetcher is what enforces "nothing enters undeclared" and a fetcher that needs a package
// installed before it can hash anything is a bootstrapping regress (the Python one was deliberately standard-library only for the same
// reason).  It is tested against the FIPS vectors AND against every hash the manifest pins on the real cached bytes.

#include <odl/devkit/bytes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace odl::devkit {

class Sha256 {
public:
    Sha256() noexcept;

    void update(ByteView data) noexcept;

    /// The digest.  The object is spent afterwards: a second call to finish() or update() is a defect of the caller.
    [[nodiscard]] std::array<std::uint8_t, 32> finish() noexcept;

private:
    void compress(const std::uint8_t* chunk) noexcept;

    std::array<std::uint32_t, 8> h_;
    std::array<std::uint8_t, 64> block_{};
    std::size_t fill_ = 0;
    std::uint64_t length_ = 0;   // bytes consumed so far
};

[[nodiscard]] std::string to_hex(ByteView bytes);

/// Lower-case hexadecimal SHA-256 of `data`.
[[nodiscard]] std::string sha256_hex(ByteView data);

/// Lower-case hexadecimal SHA-256 of a file, read in 1 MiB blocks.  Throws std::runtime_error when the file cannot be read.
[[nodiscard]] std::string sha256_file_hex(const std::filesystem::path& file);

}  // namespace odl::devkit
