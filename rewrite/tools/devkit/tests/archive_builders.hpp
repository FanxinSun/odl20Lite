#pragma once
// Archives built byte by byte from the formats' definitions, for the tests that need an archive whose contents they know: ustar (with prefix,
// GNU long names, pax headers), zip (stored and raw-deflate members), and gzip with STORED blocks (valid gzip, no compressor needed).

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/inflate.hpp>

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devkit::testing {

// ------------------------------------------------------------------------------------------------------------------------ tar builder

inline void put(Bytes& h, std::size_t off, std::string_view s) { std::memcpy(h.data() + off, s.data(), s.size()); }

inline void put_octal(Bytes& h, std::size_t off, std::size_t width, std::uint64_t v) {
    std::string digits;
    for (std::size_t i = 0; i + 1 < width; ++i) {
        digits.insert(digits.begin(), static_cast<char>('0' + (v & 7U)));
        v >>= 3;
    }
    put(h, off, digits);   // the byte after them is whatever the caller left: a NUL, or the space of the checksum field
}

inline Bytes tar_entry(const std::string& name, std::string_view content, char type = '0', const std::string& prefix = "", bool corrupt_checksum = false) {
    Bytes h(512, 0);
    put(h, 0, name);
    put(h, 100, "0000644");
    put(h, 108, "0000000");
    put(h, 116, "0000000");
    put_octal(h, 124, 12, content.size());
    put_octal(h, 136, 12, 0);
    h[156] = static_cast<std::uint8_t>(type);
    put(h, 257, "ustar");
    put(h, 263, "00");
    put(h, 345, prefix);
    std::memset(h.data() + 148, ' ', 8);
    std::uint32_t sum = 0;
    for (const std::uint8_t b : h) sum += b;
    put_octal(h, 148, 7, sum + (corrupt_checksum ? 1U : 0U));
    h[155] = ' ';
    h.insert(h.end(), content.begin(), content.end());
    h.resize((h.size() + 511) / 512 * 512, 0);
    return h;
}

inline Bytes concat(std::initializer_list<Bytes> parts, bool end_blocks = true) {
    Bytes out;
    for (const Bytes& p : parts) out.insert(out.end(), p.begin(), p.end());
    if (end_blocks) out.resize(out.size() + 1024, 0);
    return out;
}

inline std::string pax_record(const std::string& key, const std::string& value) {
    const std::string body = " " + key + "=" + value + "\n";
    std::size_t len = body.size();
    while (std::to_string(len).size() + body.size() != len) len = std::to_string(len).size() + body.size();
    return std::to_string(len) + body;
}

inline ByteView view(const Bytes& b) { return ByteView{b.data(), b.size()}; }

inline std::string text(const std::optional<Bytes>& b) { return b ? std::string(as_text(view(*b))) : std::string("<absent>"); }

// ------------------------------------------------------------------------------------------------------------------------ zip builder

inline void le16(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void le32(Bytes& b, std::uint32_t v) {
    le16(b, v & 0xFFFFU);
    le16(b, v >> 16);
}

struct ZipSpec {
    std::string name;
    Bytes stored;            // the bytes as they lie in the archive
    std::uint32_t method = 0;
    std::uint32_t crc = 0;
    std::uint32_t size = 0;  // uncompressed
    std::uint32_t flags = 0;
};

inline ZipSpec stored_file(const std::string& name, const std::string& content) {
    ZipSpec z;
    z.name = name;
    z.stored.assign(content.begin(), content.end());
    z.crc = crc32(as_bytes(content));
    z.size = static_cast<std::uint32_t>(content.size());
    return z;
}

inline Bytes make_zip(const std::vector<ZipSpec>& files, const std::string& comment = "") {
    Bytes out;
    std::vector<std::uint32_t> offsets;
    for (const ZipSpec& z : files) {
        offsets.push_back(static_cast<std::uint32_t>(out.size()));
        le32(out, 0x04034B50U);
        le16(out, 20);
        le16(out, z.flags);
        le16(out, z.method);
        le32(out, 0);   // time, date
        le32(out, z.crc);
        le32(out, static_cast<std::uint32_t>(z.stored.size()));
        le32(out, z.size);
        le16(out, static_cast<std::uint32_t>(z.name.size()));
        le16(out, 0);
        out.insert(out.end(), z.name.begin(), z.name.end());
        out.insert(out.end(), z.stored.begin(), z.stored.end());
    }
    const auto cd = static_cast<std::uint32_t>(out.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        const ZipSpec& z = files[i];
        le32(out, 0x02014B50U);
        le16(out, 20);
        le16(out, 20);
        le16(out, z.flags);
        le16(out, z.method);
        le32(out, 0);
        le32(out, z.crc);
        le32(out, static_cast<std::uint32_t>(z.stored.size()));
        le32(out, z.size);
        le16(out, static_cast<std::uint32_t>(z.name.size()));
        le16(out, 0);
        le16(out, 0);
        le16(out, 0);
        le16(out, 0);
        le32(out, 0);
        le32(out, offsets[i]);
        out.insert(out.end(), z.name.begin(), z.name.end());
    }
    const auto cd_size = static_cast<std::uint32_t>(out.size()) - cd;
    le32(out, 0x06054B50U);
    le16(out, 0);
    le16(out, 0);
    le16(out, static_cast<std::uint32_t>(files.size()));
    le16(out, static_cast<std::uint32_t>(files.size()));
    le32(out, cd_size);
    le32(out, cd);
    le16(out, static_cast<std::uint32_t>(comment.size()));
    out.insert(out.end(), comment.begin(), comment.end());
    return out;
}


/// A gzip file holding `data` in STORED deflate blocks of at most 65535 bytes: valid, and needs no compressor.
inline Bytes gzip_stored(ByteView data) {
    Bytes out = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};
    std::size_t at = 0;
    do {
        const std::size_t n = std::min<std::size_t>(65535, data.size() - at);
        const bool last = at + n == data.size();
        out.push_back(last ? 1 : 0);
        out.push_back(static_cast<std::uint8_t>(n));
        out.push_back(static_cast<std::uint8_t>(n >> 8));
        out.push_back(static_cast<std::uint8_t>(~n));
        out.push_back(static_cast<std::uint8_t>((~n) >> 8));
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(at), data.begin() + static_cast<std::ptrdiff_t>(at + n));
        at += n;
    } while (at < data.size());
    le32(out, crc32(data));
    le32(out, static_cast<std::uint32_t>(data.size()));
    return out;
}

}  // namespace odl::devkit::testing
