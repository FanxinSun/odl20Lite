#pragma once
// odl/devkit/fs.hpp — whole-file reads and writes, and a temporary directory that cleans up after itself.
//
// Every failure is a std::runtime_error whose message names the path and the reason.  Python raised OSError there and the tools caught
// it where it mattered; here the tools catch std::runtime_error at the same places.

#include <odl/devkit/bytes.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace odl::devkit {

/// The whole file.
[[nodiscard]] Bytes read_bytes(const std::filesystem::path& file);

/// The whole file as text.  It must be well-formed UTF-8 (Python's read_text(encoding="utf-8") raised UnicodeDecodeError, an uncaught crash in
/// the tools; here it is refused with a message that names the file).
[[nodiscard]] std::string read_text(const std::filesystem::path& file);

/// Writes the file, replacing it.  The parent directory must exist (Python's write_bytes did not create it either).
void write_bytes(const std::filesystem::path& file, ByteView data);
void write_text(const std::filesystem::path& file, std::string_view text);

/// A fresh, empty directory under the system's temporary directory, removed with everything in it when the object is destroyed.
class TempDir {
public:
    explicit TempDir(std::string_view prefix = "odl-tool");
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir();

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace odl::devkit
