#include <odl/devkit/fs.hpp>

#include <odl/devkit/text.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace odl::devkit {

namespace {

[[nodiscard]] std::string reason(int err) { return std::strerror(err); }

}  // namespace

Bytes read_bytes(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + file.string() + ": " + reason(errno));
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) throw std::runtime_error("cannot read " + file.string() + ": it is not a regular file");
    in.seekg(0, std::ios::beg);
    Bytes out(static_cast<std::size_t>(size));
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
