#pragma once
// odl/devkit/archive.hpp — one member out of a tar stream or a zip archive.
//
// THE MANIFEST PINS MEMBERS, NOT JUST CONTAINERS: an archive's hash says nothing about what is taken out of it (the fetcher's own rule, and
// the reason every multi-member archive declares the members it reads, each with its own SHA-256).  So what the tools need from an archive is
// exactly this: the bytes of one named member, or the refusal.
//
// Tar: ustar, GNU long names (typeflag L) and pax extended headers (x, g), header checksums verified, the LAST member of a name wins
// (Python's TarFile.getmember).  Zip: the central directory, stored and deflate members, CRC-32 and size verified, the last entry of a name
// wins; zip64, encryption and other compression methods are REFUSED, not skipped.

#include <odl/devkit/bytes.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devkit {

struct ArchiveEntry {
    std::string name;
    std::uint64_t size = 0;   // uncompressed
    bool regular = true;      // a file (not a directory, link or device)
};

/// Every member of a tar stream in order, long names resolved.  Throws FormatError.
[[nodiscard]] std::vector<ArchiveEntry> tar_entries(ByteView tar);

/// The bytes of the regular file `name` of a tar stream, or nullopt if there is no member of that name.  A member of that name that is not a
/// regular file is an error (FormatError), as is a damaged archive.
[[nodiscard]] std::optional<Bytes> tar_member(ByteView tar, std::string_view name);

/// Every entry of a zip archive, in central-directory order.  Throws FormatError.
[[nodiscard]] std::vector<ArchiveEntry> zip_entries(ByteView zip);

/// The bytes of the zip member `name`, or nullopt if absent.  Throws FormatError.
[[nodiscard]] std::optional<Bytes> zip_member(ByteView zip, std::string_view name);

}  // namespace odl::devkit
