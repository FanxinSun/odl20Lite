#include <odl/devkit/sha256.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace odl::devkit {

namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

[[nodiscard]] constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept { return (x >> n) | (x << (32U - n)); }

}  // namespace

Sha256::Sha256() noexcept
    : h_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U} {}

void Sha256::compress(const std::uint8_t* chunk) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t t = 0; t < 16; ++t) {
        w[t] = (static_cast<std::uint32_t>(chunk[4 * t]) << 24) | (static_cast<std::uint32_t>(chunk[4 * t + 1]) << 16) |
               (static_cast<std::uint32_t>(chunk[4 * t + 2]) << 8) | static_cast<std::uint32_t>(chunk[4 * t + 3]);
    }
    for (std::size_t t = 16; t < 64; ++t) {
        const std::uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        const std::uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (std::size_t t = 0; t < 64; ++t) {
        const std::uint32_t big_s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + big_s1 + ch + kRound[t] + w[t];
        const std::uint32_t big_s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = big_s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
    h_[5] += f;
    h_[6] += g;
    h_[7] += h;
}

void Sha256::update(ByteView data) noexcept {
    length_ += data.size();
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    if (fill_ > 0) {
        const std::size_t take = std::min(n, block_.size() - fill_);
        std::memcpy(block_.data() + fill_, p, take);
        fill_ += take;
        p += take;
        n -= take;
        if (fill_ < block_.size()) return;
        compress(block_.data());
        fill_ = 0;
    }
    while (n >= block_.size()) {
        compress(p);
        p += block_.size();
        n -= block_.size();
    }
    if (n > 0) {
        std::memcpy(block_.data(), p, n);
        fill_ = n;
    }
}

std::array<std::uint8_t, 32> Sha256::finish() noexcept {
    const std::uint64_t bits = length_ * 8U;
    std::array<std::uint8_t, 72> pad{};
    pad[0] = 0x80U;
    // the message, a 1 bit, zeros, then the 64-bit length: the total is a multiple of 64 bytes
    const std::size_t zeros = (fill_ < 56) ? (55 - fill_) : (119 - fill_);
    for (std::size_t i = 0; i < 8; ++i) pad[1 + zeros + i] = static_cast<std::uint8_t>(bits >> (56U - 8U * i));
    update(ByteView{pad.data(), 1 + zeros + 8});
    std::array<std::uint8_t, 32> out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[4 * i] = static_cast<std::uint8_t>(h_[i] >> 24);
        out[4 * i + 1] = static_cast<std::uint8_t>(h_[i] >> 16);
        out[4 * i + 2] = static_cast<std::uint8_t>(h_[i] >> 8);
        out[4 * i + 3] = static_cast<std::uint8_t>(h_[i]);
    }
    return out;
}

std::string to_hex(ByteView bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes) {
        out.push_back(kDigits[b >> 4]);
        out.push_back(kDigits[b & 0x0fU]);
    }
    return out;
}

std::string sha256_hex(ByteView data) {
    Sha256 h;
    h.update(data);
    const auto d = h.finish();
    return to_hex(ByteView{d.data(), d.size()});
}

std::string sha256_file_hex(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + file.string());
    Sha256 h;
    std::vector<char> block(std::size_t{1} << 20);
    while (in) {
        in.read(block.data(), static_cast<std::streamsize>(block.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) h.update(ByteView{reinterpret_cast<const std::uint8_t*>(block.data()), static_cast<std::size_t>(got)});
    }
    if (!in.eof()) throw std::runtime_error("cannot read " + file.string());
    const auto d = h.finish();
    return to_hex(ByteView{d.data(), d.size()});
}

}  // namespace odl::devkit
