#include <odl/devkit/archive.hpp>

#include <odl/devkit/inflate.hpp>

#include <algorithm>
#include <cstring>

namespace odl::devkit {

namespace {

// ---------------------------------------------------------------------------------------------------------------------------- tar

constexpr std::size_t kBlock = 512;

struct TarRecord {
    std::string name;
    char type = '0';
    std::size_t size = 0;
    std::size_t data = 0;   // offset of the member's data in the stream
};

[[nodiscard]] std::size_t tar_number(const std::uint8_t* f, std::size_t n) {
    if ((f[0] & 0x80U) != 0) {   // GNU base-256
        std::size_t v = f[0] & 0x7FU;
        for (std::size_t i = 1; i < n; ++i) v = (v << 8) | f[i];
        return v;
    }
    std::size_t v = 0;
    std::size_t i = 0;
    while (i < n && (f[i] == ' ' || f[i] == 0)) ++i;
    for (; i < n && f[i] >= '0' && f[i] <= '7'; ++i) v = (v << 3) | static_cast<std::size_t>(f[i] - '0');
    for (; i < n; ++i) {
        if (f[i] != ' ' && f[i] != 0) throw FormatError("tar: a numeric header field is not octal");
    }
    return v;
}

[[nodiscard]] std::string tar_string(const std::uint8_t* f, std::size_t n) {
    std::size_t len = 0;
    while (len < n && f[len] != 0) ++len;
    return std::string(reinterpret_cast<const char*>(f), len);
}

[[nodiscard]] bool checksum_ok(const std::uint8_t* h) {
    std::uint32_t unsigned_sum = 0;
    std::int32_t signed_sum = 0;
    for (std::size_t i = 0; i < kBlock; ++i) {
        const std::uint8_t b = (i >= 148 && i < 156) ? static_cast<std::uint8_t>(' ') : h[i];
        unsigned_sum += b;
        signed_sum += static_cast<std::int8_t>(b);
    }
    const std::size_t want = tar_number(h + 148, 8);
    return want == unsigned_sum || static_cast<std::int64_t>(want) == signed_sum;
}

// The records of a pax extended header: "<length> <key>=<value>\n", the length counting the whole record.
void pax_records(ByteView data, std::string* path, std::size_t* size, bool* have_size) {
    std::size_t at = 0;
    while (at < data.size()) {
        std::size_t len = 0;
        std::size_t k = at;
        while (k < data.size() && data[k] >= '0' && data[k] <= '9') len = len * 10 + (data[k++] - '0');
        if (k == at || k >= data.size() || data[k] != ' ' || len == 0 || at + len > data.size() || data[at + len - 1] != '\n') {
            throw FormatError("tar: a damaged pax header record");
        }
        const std::string_view rec = as_text(data.subspan(k + 1, at + len - 1 - (k + 1)));
        const std::size_t eq = rec.find('=');
        if (eq == std::string_view::npos) throw FormatError("tar: a pax record has no '='");
        const std::string_view key = rec.substr(0, eq);
        const std::string_view value = rec.substr(eq + 1);
        if (key == "path" && path != nullptr) *path = std::string(value);
        if (key == "size" && size != nullptr) {
            std::size_t v = 0;
            for (const char c : value) {
                if (c < '0' || c > '9') throw FormatError("tar: a pax size is not a number");
                v = v * 10 + static_cast<std::size_t>(c - '0');
            }
            *size = v;
            if (have_size != nullptr) *have_size = true;
        }
        at += len;
    }
}

[[nodiscard]] std::vector<TarRecord> tar_records(ByteView tar) {
    std::vector<TarRecord> out;
    std::size_t off = 0;
    std::string long_name;
    bool have_long = false;
    std::string pax_path;
    bool have_pax_path = false;
    std::size_t pax_size = 0;
    bool have_pax_size = false;
    while (off + kBlock <= tar.size()) {
        const std::uint8_t* h = tar.data() + off;
        if (std::all_of(h, h + kBlock, [](std::uint8_t b) { return b == 0; })) break;   // the end-of-archive block
        if (!checksum_ok(h)) throw FormatError("tar: a bad header checksum at offset " + std::to_string(off));
        std::string name = tar_string(h, 100);
        std::size_t size = tar_number(h + 124, 12);
        const char type = static_cast<char>(h[156]);
        const bool ustar = std::memcmp(h + 257, "ustar", 5) == 0;
        const std::string prefix = ustar ? tar_string(h + 345, 155) : std::string();
        const std::size_t data = off + kBlock;
        const auto padded = [](std::size_t n) { return (n + (kBlock - 1)) / kBlock * kBlock; };
        if (size > tar.size() || data + padded(size) > tar.size() + (kBlock - 1)) throw FormatError("tar: the archive is truncated inside a member");
        const ByteView body = tar.subspan(data, std::min<std::size_t>(size, tar.size() - data));
        if (type == 'x') {
            pax_records(body, &pax_path, &pax_size, &have_pax_size);
            have_pax_path = !pax_path.empty();
            off = data + padded(size);
            continue;
        }
        if (type == 'g') {   // global pax headers (the commit comment of a GitHub archive): nothing the tools read
            off = data + padded(size);
            continue;
        }
        if (type == 'L') {
            long_name = tar_string(body.data(), body.size());
            have_long = true;
            off = data + padded(size);
            continue;
        }
        if (type == 'K') {
            off = data + padded(size);
            continue;
        }
        if (!prefix.empty()) name = prefix + "/" + name;
        if (have_long) name = long_name;
        if (have_pax_path) name = pax_path;
        if (have_pax_size) size = pax_size;
        if (type == '5' || (type == '0' && !name.empty() && name.back() == '/')) {
            while (!name.empty() && name.back() == '/') name.pop_back();
        }
        if (data + size > tar.size()) throw FormatError("tar: the archive is truncated inside a member");
        out.push_back(TarRecord{std::move(name), type, size, data});
        have_long = have_pax_path = have_pax_size = false;
        long_name.clear();
        pax_path.clear();
        off = data + padded(size);
    }
    return out;
}

[[nodiscard]] bool is_regular(char type) noexcept { return type == '0' || type == '\0' || type == '7'; }

// ---------------------------------------------------------------------------------------------------------------------------- zip

[[nodiscard]] std::uint32_t le16(const std::uint8_t* p) noexcept { return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8); }
[[nodiscard]] std::uint32_t le32(const std::uint8_t* p) noexcept { return le16(p) | (le16(p + 2) << 16); }

struct ZipRecord {
    std::string name;
    std::uint32_t method = 0;
    std::uint32_t flags = 0;
    std::uint32_t crc = 0;
    std::uint64_t comp = 0;
    std::uint64_t size = 0;
    std::uint64_t local = 0;
};

[[nodiscard]] std::vector<ZipRecord> zip_records(ByteView zip) {
    if (zip.size() < 22) throw FormatError("zip: the file is too short to be an archive");
    // the end-of-central-directory record: the last 22 + comment bytes
    std::size_t eocd = std::string::npos;
    const std::size_t lowest = zip.size() > 22 + 65535 ? zip.size() - (22 + 65535) : 0;
    for (std::size_t at = zip.size() - 22 + 1; at-- > lowest;) {
        if (le32(zip.data() + at) == 0x06054B50U) {
            eocd = at;
            break;
        }
    }
    if (eocd == std::string::npos) throw FormatError("zip: no end-of-central-directory record: not a zip archive");
    const std::uint8_t* e = zip.data() + eocd;
    const std::uint32_t count = le16(e + 10);
    const std::uint32_t cd_size = le32(e + 12);
    const std::uint32_t cd_off = le32(e + 16);
    if (count == 0xFFFFU || cd_size == 0xFFFFFFFFU || cd_off == 0xFFFFFFFFU) throw FormatError("zip: zip64 archives are not supported");
    if (le16(e + 4) != 0 || le16(e + 6) != 0) throw FormatError("zip: multi-disk archives are not supported");
    if (static_cast<std::uint64_t>(cd_off) + cd_size > eocd) throw FormatError("zip: the central directory lies outside the file");
    std::vector<ZipRecord> out;
    std::size_t at = cd_off;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (at + 46 > zip.size() || le32(zip.data() + at) != 0x02014B50U) throw FormatError("zip: a damaged central directory");
        const std::uint8_t* c = zip.data() + at;
        const std::size_t name_len = le16(c + 28);
        const std::size_t extra_len = le16(c + 30);
        const std::size_t comment_len = le16(c + 32);
        if (at + 46 + name_len + extra_len + comment_len > zip.size()) throw FormatError("zip: a damaged central directory entry");
        ZipRecord r;
        r.flags = le16(c + 8);
        r.method = le16(c + 10);
        r.crc = le32(c + 16);
        r.comp = le32(c + 20);
        r.size = le32(c + 24);
        r.local = le32(c + 42);
        if (r.comp == 0xFFFFFFFFU || r.size == 0xFFFFFFFFU || r.local == 0xFFFFFFFFU) throw FormatError("zip: zip64 entries are not supported");
        r.name.assign(reinterpret_cast<const char*>(c + 46), name_len);
        out.push_back(std::move(r));
        at += 46 + name_len + extra_len + comment_len;
    }
    return out;
}

}  // namespace

std::vector<ArchiveEntry> tar_entries(ByteView tar) {
    std::vector<ArchiveEntry> out;
    for (const TarRecord& r : tar_records(tar)) out.push_back(ArchiveEntry{r.name, r.size, is_regular(r.type)});
    return out;
}

std::optional<Bytes> tar_member(ByteView tar, std::string_view name) {
    const std::vector<TarRecord> records = tar_records(tar);
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        if (it->name != name) continue;
        if (!is_regular(it->type)) throw FormatError("tar: the member '" + it->name + "' is not a regular file");
        return Bytes(tar.begin() + static_cast<std::ptrdiff_t>(it->data), tar.begin() + static_cast<std::ptrdiff_t>(it->data + it->size));
    }
    return std::nullopt;
}

std::vector<ArchiveEntry> zip_entries(ByteView zip) {
    std::vector<ArchiveEntry> out;
    for (const ZipRecord& r : zip_records(zip)) out.push_back(ArchiveEntry{r.name, r.size, r.name.empty() || r.name.back() != '/'});
    return out;
}

std::optional<Bytes> zip_member(ByteView zip, std::string_view name) {
    const std::vector<ZipRecord> records = zip_records(zip);
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        if (it->name != name) continue;
        if ((it->flags & 0x1U) != 0) throw FormatError("zip: the member '" + it->name + "' is encrypted");
        const std::size_t at = static_cast<std::size_t>(it->local);
        if (at + 30 > zip.size() || le32(zip.data() + at) != 0x04034B50U) throw FormatError("zip: a damaged local header for '" + it->name + "'");
        const std::size_t data = at + 30 + le16(zip.data() + at + 26) + le16(zip.data() + at + 28);
        if (data + it->comp > zip.size()) throw FormatError("zip: the data of '" + it->name + "' lies outside the file");
        const ByteView body = zip.subspan(data, static_cast<std::size_t>(it->comp));
        Bytes out;
        if (it->method == 0) {
            out.assign(body.begin(), body.end());
        } else if (it->method == 8) {
            out = inflate(body, nullptr, static_cast<std::size_t>(it->size));
        } else {
            throw FormatError("zip: compression method " + std::to_string(it->method) + " of '" + it->name + "' is not supported");
        }
        if (out.size() != it->size) throw FormatError("zip: the size of '" + it->name + "' is not what its entry declares");
        if (crc32(ByteView{out.data(), out.size()}) != it->crc) throw FormatError("zip: bad CRC-32 for file '" + it->name + "'");
        return out;
    }
    return std::nullopt;
}

}  // namespace odl::devkit
