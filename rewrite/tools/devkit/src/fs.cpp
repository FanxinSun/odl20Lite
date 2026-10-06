#include <odl/devkit/fs.hpp>

#include <odl/devkit/text.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <stdexcept>
#include <system_error>

namespace odl::devkit {

namespace {

[[nodiscard]] std::string reason(int err) { return std::strerror(err); }

}  // namespace

Bytes read_bytes(const std::filesystem::path& file) {
    // WHAT A FILE IS IS DECIDED BEFORE IT IS OPENED, by its type and not by what the filesystem says about its size.  C3 read the size of a DIRECTORY (a source tree may hold one called
    // x.cpp) as "seek to the end, tell": tmpfs answers -1 there, ext4 -- GitHub's runner, whose /tmp is a disk -- answers 2^63-1, the vector of that size threw std::bad_alloc, which is no
    // std::runtime_error, and speccheck exited 70 on the runner and 0 here (plan L0 step 8, group C4, PROVENANCE section 41.10).  A FIFO would not even be opened: it blocks.
    std::error_code kind_error;
    const std::filesystem::file_status kind = std::filesystem::status(file, kind_error);   // follows a link, as opening it does
    if (kind_error) throw std::runtime_error("cannot read " + file.string() + ": " + kind_error.message());
    if (!std::filesystem::is_regular_file(kind)) throw std::runtime_error("cannot read " + file.string() + ": it is not a regular file");
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + file.string() + ": " + reason(errno));
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) throw std::runtime_error("cannot read " + file.string() + ": it is not a regular file");
    in.seekg(0, std::ios::beg);
    Bytes out;
    try {
        out.resize(static_cast<std::size_t>(size));
    } catch (const std::bad_alloc&) {   // a file too large for the memory there is is still a refusal that names the file, never an internal error
        throw std::runtime_error("cannot read " + file.string() + ": it is too large to read into memory");
    }
    if (!out.empty()) in.read(reinterpret_cast<char*>(out.data()), size);
    if (!in) throw std::runtime_error("cannot read " + file.string() + ": a read failed");
    return out;
}

std::string read_text(const std::filesystem::path& file) {
    const Bytes b = read_bytes(file);
    std::string text(as_text(ByteView{b.data(), b.size()}));
    if (!valid_utf8(text)) throw std::runtime_error(file.string() + " is not well-formed UTF-8");
    return text;
}

std::string path_suffix(std::string_view name) {
    const std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= name.size()) return std::string();
    return std::string(name.substr(dot));
}

std::vector<std::vector<std::string>> files_below(const std::filesystem::path& base, const std::function<bool(const std::string&)>& skip_directory) {
    namespace fs = std::filesystem;
    std::vector<std::vector<std::string>> found;
    std::string trail;   // the components below `base` of the directory being read, joined by '/' (a vector of them draws a false -Wnull-dereference from GCC 15)
    const auto walk = [&](const auto& self, const fs::path& dir) -> void {
        // A directory that cannot be read, or is no directory at all (base included: a file, an absent path, an empty one), gives the end iterator at once and is passed over; so does what cannot
        // be examined.  The error codes are therefore not looked at.
        std::error_code ignored;
        for (fs::directory_iterator it(dir, ignored), end; it != end; it.increment(ignored)) {
            const std::string name = it->path().filename().string();
            if (!it->is_symlink(ignored) && it->is_directory(ignored)) {
                if (skip_directory && skip_directory(name)) continue;
                const std::size_t keep = trail.size();
                if (!trail.empty()) trail += '/';
                trail += name;
                self(self, it->path());
                trail.resize(keep);
            } else if (it->is_regular_file(ignored)) {   // follows a link: a link to a regular file is that file
                const std::string whole = trail.empty() ? name : trail + '/' + name;
                std::vector<std::string> parts;
                std::size_t from = 0;
                while (true) {
                    const std::size_t slash = whole.find('/', from);
                    parts.push_back(whole.substr(from, slash == std::string::npos ? std::string::npos : slash - from));
                    if (slash == std::string::npos) break;
                    from = slash + 1;
                }
                found.push_back(std::move(parts));
            }
        }
    };
    walk(walk, base);
    std::sort(found.begin(), found.end());
    return found;
}

void write_bytes(const std::filesystem::path& file, ByteView data) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + file.string() + ": " + reason(errno));
    if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    out.close();
    if (!out) throw std::runtime_error("cannot write " + file.string() + ": a write failed");
}

void write_text(const std::filesystem::path& file, std::string_view text) { write_bytes(file, as_bytes(text)); }

TempDir::TempDir(std::string_view prefix) {
    std::string pattern = (std::filesystem::temp_directory_path() / (std::string(prefix) + "-XXXXXX")).string();
    if (::mkdtemp(pattern.data()) == nullptr) throw std::runtime_error("cannot create a temporary directory: " + reason(errno));
    path_ = pattern;
}

TempDir::~TempDir() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
}

}  // namespace odl::devkit
