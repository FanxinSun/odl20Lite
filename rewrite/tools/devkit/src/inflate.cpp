#include <odl/devkit/inflate.hpp>

#include <array>
#include <cstring>
#include <string>

namespace odl::devkit {

namespace {

// ---------------------------------------------------------------------------------------------------------------------------- CRC-32

constexpr std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1U) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        t[n] = c;
    }
    return t;
}
constexpr std::array<std::uint32_t, 256> kCrcTable = make_crc_table();

// ---------------------------------------------------------------------------------------------------------------------------- bits

// The bits of a DEFLATE stream, least significant bit first.  Reading past the end supplies zeros and is remembered: the decoder asks
// overrun() after each symbol, so a truncated stream is an error and never a plausible-looking tail.
class Bits {
public:
    explicit Bits(ByteView in) noexcept : start_(in.data()), p_(in.data()), end_(in.data() + in.size()) {}

    void refill() noexcept {
        while (count_ <= 56) {
            std::uint64_t b = 0;
            if (p_ < end_) {
                b = *p_++;
            } else {
                ++pad_;
            }
            buf_ |= b << count_;
            count_ += 8;
        }
    }

    [[nodiscard]] std::uint32_t peek(unsigned n) noexcept {
        if (count_ < n) refill();
        return static_cast<std::uint32_t>(buf_ & ((std::uint64_t{1} << n) - 1));
    }

    void drop(unsigned n) noexcept {
        buf_ >>= n;
        count_ -= n;
    }

    [[nodiscard]] std::uint32_t get(unsigned n) noexcept {
        if (n == 0) return 0;
        const std::uint32_t v = peek(n);
        drop(n);
        return v;
    }

    /// Have bits been consumed that the input did not hold?
    [[nodiscard]] bool overrun() const noexcept { return count_ < pad_ * 8U; }

    void align() noexcept { drop(count_ % 8U); }

    /// Whole bytes the stream has used so far (call after align()).
    [[nodiscard]] std::size_t consumed() const noexcept { return static_cast<std::size_t>(p_ - start_) - (count_ - pad_ * 8U) / 8U; }

    /// After align(): copies `n` stored bytes to `out`.
    void copy_stored(Bytes& out, std::size_t n) {
        // first the real bytes the bit buffer already holds (the virtual ones sit at its top), then straight from the input
        while (n > 0 && count_ > pad_ * 8U) {
            out.push_back(static_cast<std::uint8_t>(buf_ & 0xFFU));
            drop(8);
            --n;
        }
        if (n == 0) return;
        if (pad_ != 0 || static_cast<std::size_t>(end_ - p_) < n) throw FormatError("deflate: a stored block runs past the end of the input");
        out.insert(out.end(), p_, p_ + n);
        p_ += n;
    }

private:
    const std::uint8_t* start_;
    const std::uint8_t* p_;
    const std::uint8_t* end_;
    std::uint64_t buf_ = 0;
    unsigned count_ = 0;
    unsigned pad_ = 0;   // virtual bytes appended past the end of the input
};

// ---------------------------------------------------------------------------------------------------------------------------- Huffman

constexpr unsigned kFastBits = 10;

struct Huffman {
    std::array<std::uint16_t, std::size_t{1} << kFastBits> fast{};   // (symbol << 4) | length, 0 where the code is longer than kFastBits
    std::array<std::uint16_t, 16> count{};                            // codes of each length
    std::array<std::uint16_t, 288> symbol{};                          // symbols in canonical order
};

[[nodiscard]] std::uint32_t reverse_bits(std::uint32_t v, unsigned n) noexcept {
    std::uint32_t r = 0;
    for (unsigned i = 0; i < n; ++i) {
        r = (r << 1) | (v & 1U);
        v >>= 1;
    }
    return r;
}

// Builds the decoding tables for `lengths` (0 = unused).  Refuses an over-subscribed code, and an incomplete one unless it is the single code of
// length one that DEFLATE permits (a distance tree with one code).
void build(Huffman& h, const std::uint8_t* lengths, std::size_t n) {
    h = Huffman{};
    for (std::size_t s = 0; s < n; ++s) ++h.count[lengths[s]];
    h.count[0] = 0;
    int left = 1;
    unsigned max_len = 0;
    for (unsigned len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) throw FormatError("deflate: an over-subscribed Huffman code");
        if (h.count[len] != 0) max_len = len;
    }
    if (left > 0 && max_len != 0 && !(max_len == 1 && h.count[1] == 1)) throw FormatError("deflate: an incomplete Huffman code");
    std::array<std::uint16_t, 16> offs{};
    for (unsigned len = 1; len < 15; ++len) offs[len + 1] = static_cast<std::uint16_t>(offs[len] + h.count[len]);
    for (std::size_t s = 0; s < n; ++s) {
        if (lengths[s] != 0) h.symbol[offs[lengths[s]]++] = static_cast<std::uint16_t>(s);
    }
    // the canonical codes, and the fast table for the short ones
    std::uint32_t code = 0;
    std::size_t index = 0;
    for (unsigned len = 1; len <= 15; ++len) {
        for (unsigned k = 0; k < h.count[len]; ++k, ++code, ++index) {
            if (len > kFastBits) continue;
            const std::uint32_t rev = reverse_bits(code, len);
            const auto entry = static_cast<std::uint16_t>((h.symbol[index] << 4U) | len);
            for (std::uint32_t fill = rev; fill < (1U << kFastBits); fill += (1U << len)) h.fast[fill] = entry;
        }
        code <<= 1;
    }
}

// One symbol, or -1 if the bits are not a code of this tree.
[[nodiscard]] int decode(Bits& bits, const Huffman& h) noexcept {
    const std::uint32_t v = bits.peek(15);
    const std::uint16_t e = h.fast[v & ((1U << kFastBits) - 1)];
    if (e != 0) {
        bits.drop(e & 15U);
        return e >> 4U;
    }
    // canonical decoding, bit by bit, for the codes longer than the fast table
    std::int32_t code = 0;
    std::int32_t first = 0;
    std::int32_t index = 0;
    for (unsigned len = 1; len <= 15; ++len) {
        code |= static_cast<std::int32_t>((v >> (len - 1)) & 1U);
        const std::int32_t cnt = h.count[len];
        if (code - cnt < first) {
            bits.drop(len);
            return h.symbol[static_cast<std::size_t>(index + (code - first))];
        }
        index += cnt;
        first += cnt;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

constexpr std::array<std::uint16_t, 29> kLenBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLenExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                                     193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
constexpr std::array<std::uint8_t, 19> kClOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

void codes(Bits& bits, Bytes& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        if (bits.overrun()) throw FormatError("deflate: the stream ends inside a block");
        const int sym = decode(bits, lit);
        if (sym < 0) throw FormatError("deflate: an invalid literal/length code");
        if (sym < 256) {
            out.push_back(static_cast<std::uint8_t>(sym));
            continue;
        }
        if (sym == 256) return;
        const auto li = static_cast<std::size_t>(sym - 257);
        if (li >= kLenBase.size()) throw FormatError("deflate: an invalid length code");
        const std::size_t len = kLenBase[li] + bits.get(kLenExtra[li]);
        const int dsym = decode(bits, dist);
        if (dsym < 0 || static_cast<std::size_t>(dsym) >= kDistBase.size()) throw FormatError("deflate: an invalid distance code");
        const std::size_t d = kDistBase[static_cast<std::size_t>(dsym)] + bits.get(kDistExtra[static_cast<std::size_t>(dsym)]);
        if (d > out.size()) throw FormatError("deflate: a distance reaches before the start of the output");
        const std::size_t at = out.size();
        out.resize(at + len);
        std::uint8_t* to = out.data() + at;
        const std::uint8_t* from = to - d;
        for (std::size_t k = 0; k < len; ++k) to[k] = from[k];   // may overlap: this is how DEFLATE repeats
    }
}

[[nodiscard]] const Huffman& fixed_literals() {
    static const Huffman h = [] {
        std::array<std::uint8_t, 288> l{};
        for (std::size_t i = 0; i < 288; ++i) l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        Huffman t;
        build(t, l.data(), l.size());
        return t;
    }();
    return h;
}

[[nodiscard]] const Huffman& fixed_distances() {
    static const Huffman h = [] {
        std::array<std::uint8_t, 32> l{};
        l.fill(5);
        Huffman t;
        build(t, l.data(), l.size());
        return t;
    }();
    return h;
}

void dynamic_block(Bits& bits, Bytes& out) {
    const std::size_t nlen = bits.get(5) + 257;
    const std::size_t ndist = bits.get(5) + 1;
    const std::size_t ncode = bits.get(4) + 4;
    if (nlen > 286 || ndist > 30) throw FormatError("deflate: too many length or distance codes");
    std::array<std::uint8_t, 19> cl{};
    for (std::size_t i = 0; i < ncode; ++i) cl[kClOrder[i]] = static_cast<std::uint8_t>(bits.get(3));
    Huffman clh;
    build(clh, cl.data(), cl.size());
    std::array<std::uint8_t, 320> lengths{};
    std::size_t at = 0;
    while (at < nlen + ndist) {
        if (bits.overrun()) throw FormatError("deflate: the stream ends inside a block header");
        const int sym = decode(bits, clh);
        if (sym < 0) throw FormatError("deflate: an invalid code-length code");
        if (sym < 16) {
            lengths[at++] = static_cast<std::uint8_t>(sym);
            continue;
        }
        std::uint8_t value = 0;
        std::size_t rep = 0;
        if (sym == 16) {
            if (at == 0) throw FormatError("deflate: a repeat with no previous length");
            value = lengths[at - 1];
            rep = 3 + bits.get(2);
        } else if (sym == 17) {
            rep = 3 + bits.get(3);
        } else {
            rep = 11 + bits.get(7);
        }
        if (at + rep > nlen + ndist) throw FormatError("deflate: a repeat runs past the code lengths");
        while (rep-- > 0) lengths[at++] = value;
    }
    if (lengths[256] == 0) throw FormatError("deflate: no end-of-block code");
    Huffman lit;
    Huffman dist;
    build(lit, lengths.data(), nlen);
    build(dist, lengths.data() + nlen, ndist);
    codes(bits, out, lit, dist);
}

}  // namespace

std::uint32_t crc32(ByteView data, std::uint32_t crc) noexcept {
    std::uint32_t c = ~crc;
    for (const std::uint8_t b : data) c = kCrcTable[(c ^ b) & 0xFFU] ^ (c >> 8);
    return ~c;
}

Bytes inflate(ByteView data, std::size_t* consumed, std::size_t size_hint) {
    Bits bits(data);
    Bytes out;
    out.reserve(size_hint);
    bool last = false;
    while (!last) {
        last = bits.get(1) != 0;
        const std::uint32_t type = bits.get(2);
        if (bits.overrun()) throw FormatError("deflate: the stream ends inside a block header");
        if (type == 0) {
            bits.align();
            const std::uint32_t len = bits.get(16);
            const std::uint32_t nlen = bits.get(16);
            if (bits.overrun()) throw FormatError("deflate: the stream ends inside a stored block header");
            if ((len ^ 0xFFFFU) != nlen) throw FormatError("deflate: a stored block's length and its complement disagree");
            bits.copy_stored(out, len);
        } else if (type == 1) {
            codes(bits, out, fixed_literals(), fixed_distances());
        } else if (type == 2) {
            dynamic_block(bits, out);
        } else {
            throw FormatError("deflate: an invalid block type");
        }
        if (bits.overrun()) throw FormatError("deflate: the stream is truncated");
    }
    bits.align();
    if (consumed != nullptr) *consumed = bits.consumed();
    return out;
}

Bytes gunzip(ByteView data) {
    Bytes out;
    std::size_t pos = 0;
    bool first = true;
    for (;;) {
        // zero padding AFTER a member is allowed, as Python's gzip allows it; before the first member a zero byte is not a gzip file
        if (!first) {
            while (pos < data.size() && data[pos] == 0) ++pos;
            if (pos >= data.size()) return out;
        }
        if (data.empty()) throw FormatError("gzip: no data");
        first = false;
        const std::uint8_t* h = data.data() + pos;
        const std::size_t avail = data.size() - pos;
        if (avail < 18 || h[0] != 0x1F || h[1] != 0x8B) throw FormatError("gzip: not a gzipped file (the member header is missing)");
        if (h[2] != 8) throw FormatError("gzip: unknown compression method");
        const std::uint8_t flags = h[3];
        if ((flags & 0xE0U) != 0) throw FormatError("gzip: reserved header flags are set");
        std::size_t q = 10;
        if ((flags & 0x04U) != 0) {   // FEXTRA
            if (q + 2 > avail) throw FormatError("gzip: the extra field is truncated");
            q += 2 + (static_cast<std::size_t>(h[q]) | (static_cast<std::size_t>(h[q + 1]) << 8));
        }
        for (const std::uint8_t flag : {std::uint8_t{0x08}, std::uint8_t{0x10}}) {   // FNAME, FCOMMENT: zero-terminated
            if ((flags & flag) == 0) continue;
            while (q < avail && h[q] != 0) ++q;
            ++q;
        }
        if ((flags & 0x02U) != 0) q += 2;   // FHCRC
        if (q > avail) throw FormatError("gzip: the member header is truncated");
        std::size_t used = 0;
        Bytes member = inflate(ByteView{h + q, avail - q}, &used);
        if (avail - q - used < 8) throw FormatError("gzip: the member trailer is truncated");
        const std::uint8_t* t = h + q + used;
        const std::uint32_t want_crc = static_cast<std::uint32_t>(t[0]) | (static_cast<std::uint32_t>(t[1]) << 8) |
                                       (static_cast<std::uint32_t>(t[2]) << 16) | (static_cast<std::uint32_t>(t[3]) << 24);
        const std::uint32_t want_len = static_cast<std::uint32_t>(t[4]) | (static_cast<std::uint32_t>(t[5]) << 8) |
                                       (static_cast<std::uint32_t>(t[6]) << 16) | (static_cast<std::uint32_t>(t[7]) << 24);
        if (crc32(ByteView{member.data(), member.size()}) != want_crc) throw FormatError("gzip: CRC check failed");
        if (static_cast<std::uint32_t>(member.size() & 0xFFFFFFFFU) != want_len) throw FormatError("gzip: incorrect length of data produced");
        out.insert(out.end(), member.begin(), member.end());
        pos += q + used + 8;
    }
}

}  // namespace odl::devkit
