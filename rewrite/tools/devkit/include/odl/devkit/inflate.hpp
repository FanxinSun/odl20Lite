#pragma once
// odl/devkit/inflate.hpp — DEFLATE (RFC 1951), gzip (RFC 1952) and CRC-32: how the tools read a pinned archive.
//
// WHY THESE ARE HERE AND NOT A LIBRARY.  The manifest pins archives by hash BEFORE anything parses them, so the decoder only ever sees bytes
// that are known, and a decoder of a hundred and fifty lines that is tested on every pinned member (each member carries its own SHA-256 in
// the manifest) is a better dependency than a host library a clean clone does not promise.  Python's gzip, tarfile and zipfile refused a bad
// CRC, a short stream and a trailing byte that was not a member; so does this.

#include <odl/devkit/bytes.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace odl::devkit {

/// Thrown for every malformed archive, stream or header: a corrupt input is refused with a message, never decoded into something plausible.
struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320), the one gzip and zip carry.  `crc` is the value of the bytes before `data`.
[[nodiscard]] std::uint32_t crc32(ByteView data, std::uint32_t crc = 0) noexcept;

/// A raw DEFLATE stream.  `consumed`, if given, receives the number of input bytes the stream occupied (the last partial byte included).
/// `size_hint` only reserves the output.  Throws FormatError.
[[nodiscard]] Bytes inflate(ByteView data, std::size_t* consumed = nullptr, std::size_t size_hint = 0);

/// A gzip file: every member in turn, concatenated, each with its CRC-32 and length checked; zero padding after a member is skipped (as
/// Python's gzip does) and any other trailing byte must begin another member.  Throws FormatError.
[[nodiscard]] Bytes gunzip(ByteView data);

}  // namespace odl::devkit
