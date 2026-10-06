#pragma once
// odl/devkit/fs.hpp — whole-file reads and writes, and a temporary directory that cleans up after itself.
//
// Every failure is a std::runtime_error whose message names the path and the reason.  Python raised OSError there and the tools caught
// it where it mattered; here the tools catch std::runtime_error at the same places.

#include <odl/devkit/bytes.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devkit {

/// The whole file.
[[nodiscard]] Bytes read_bytes(const std::filesystem::path& file);

/// The whole file as text.  It must be well-formed UTF-8 (Python's read_text(encoding="utf-8") raised UnicodeDecodeError, an uncaught crash in
/// the tools; here it is refused with a message that names the file).
[[nodiscard]] std::string read_text(const std::filesystem::path& file);


/// pathlib's PurePath.suffix of a final path component (Python 3.13): the text from the LAST dot when that dot is neither the first nor the last character of the name, otherwise empty
/// (".hpp" has no suffix, "x." has none, "a.tar.gz" has ".gz").
[[nodiscard]] std::string path_suffix(std::string_view name);

/// What pathlib's `base.rglob("*")` finds that a tool can read: every REGULAR file below `base` (a link to a regular file counts; a directory, a link to nothing and a FIFO do not), as the path
/// components below `base`, in the order pathlib sorts paths (component by component, so "a/b" comes before "a-b/c").  A directory is entered unless `skip_directory` says its NAME is to be
/// skipped (the Python tools tested `"build" not in p.parts`, and so no file below a directory of that name is listed); a link to a directory is never entered (rglob does not follow one).
/// A directory that cannot be opened is passed over.  Empty when `base` is no directory.
[[nodiscard]] std::vector<std::vector<std::string>> files_below(const std::filesystem::path& base, const std::function<bool(const std::string&)>& skip_directory = {});

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
