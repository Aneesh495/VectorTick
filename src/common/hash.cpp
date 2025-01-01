#include "vectortick/common/hash.hpp"
#include <cstring>

namespace vectortick {

// SHA-256 constants
static constexpr u32 K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x74f1c51a, 0x7aa83914, 0x8e9e8e9e, 0x64d98b3d,
    0x72c7d4d5, 0x9ae98b91, 0xf0d1e9c4, 0xd5a79147,
};

inline u32 rotr(u32 x, u32 n) noexcept {
    return (x >> n) | (x << (32 - n));
}

inline u32 choose(u32 e, u32 f, u32 g) noexcept {
    return (e & f) ^ (~e & g);
}

inline u32 majority(u32 a, u32 b, u32 c) noexcept {
    return (a & b) ^ (a & c) ^ (b & c);
}

inline u32 sigma0(u32 x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline u32 sigma1(u32 x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline u32 gamma0(u32 x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline u32 gamma1(u32 x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

Hash256 Sha256::compute(const byte* data, usize length) noexcept {
    Sha256 hasher;
    hasher.update(data, length);
    return hasher.finalize();
}

void Sha256::update(const byte* data, usize length) noexcept {
    if (finalized_) {
        reset();
    }
    
    total_len_ += length;
    
    // Process buffered data first
    while (length > 0 && buffer_len_ < 64) {
        buffer_[buffer_len_++] = static_cast<u8>(*data++);
        --length;
    }
    
    // Process full blocks from buffer
    if (buffer_len_ == 64) {
        process_block(buffer_);
        buffer_len_ = 0;
    }
    
    // Process full blocks directly from input
    while (length >= 64) {
        process_block(reinterpret_cast<const u8*>(data));
        data += 64;
        length -= 64;
    }
    
    // Buffer remaining bytes
    while (length > 0) {
        buffer_[buffer_len_++] = static_cast<u8>(*data++);
        --length;
    }
}

Hash256 Sha256::finalize() noexcept {
    if (finalized_) {
        Hash256 result;
        for (int i = 0; i < 8; ++i) {
            result.data[i * 4] = static_cast<u8>(state_[i] >> 24);
            result.data[i * 4 + 1] = static_cast<u8>(state_[i] >> 16);
            result.data[i * 4 + 2] = static_cast<u8>(state_[i] >> 8);
            result.data[i * 4 + 3] = static_cast<u8>(state_[i]);
        }
        return result;
    }
    
    // Pad message
    u64 bit_len = total_len_ * 8;
    
    // Append 1 bit
    buffer_[buffer_len_++] = 0x80;
    
    // Pad to 56 bytes (leaving room for length)
    while (buffer_len_ < 56) {
        buffer_[buffer_len_++] = 0;
    }
    
    // Append length in bits (big-endian)
    buffer_[56] = static_cast<u8>(bit_len >> 56);
    buffer_[57] = static_cast<u8>(bit_len >> 48);
    buffer_[58] = static_cast<u8>(bit_len >> 40);
    buffer_[59] = static_cast<u8>(bit_len >> 32);
    buffer_[60] = static_cast<u8>(bit_len >> 24);
    buffer_[61] = static_cast<u8>(bit_len >> 16);
    buffer_[62] = static_cast<u8>(bit_len >> 8);
    buffer_[63] = static_cast<u8>(bit_len);
    
    process_block(buffer_);
    
    // Handle case where padding spilled into second block
    if (buffer_len_ > 56) {
        buffer_len_ = 0;
        while (buffer_len_ < 56) {
            buffer_[buffer_len_++] = 0;
        }
        buffer_[56] = static_cast<u8>(bit_len >> 56);
        buffer_[57] = static_cast<u8>(bit_len >> 48);
        buffer_[58] = static_cast<u8>(bit_len >> 40);
        buffer_[59] = static_cast<u8>(bit_len >> 32);
        buffer_[60] = static_cast<u8>(bit_len >> 24);
        buffer_[61] = static_cast<u8>(bit_len >> 16);
        buffer_[62] = static_cast<u8>(bit_len >> 8);
        buffer_[63] = static_cast<u8>(bit_len);
        process_block(buffer_);
    }
    
    finalized_ = true;
    
    // Extract hash
    Hash256 result;
    for (int i = 0; i < 8; ++i) {
        result.data[i * 4] = static_cast<u8>(state_[i] >> 24);
        result.data[i * 4 + 1] = static_cast<u8>(state_[i] >> 16);
        result.data[i * 4 + 2] = static_cast<u8>(state_[i] >> 8);
        result.data[i * 4 + 3] = static_cast<u8>(state_[i]);
    }
    return result;
}

void Sha256::reset() noexcept {
    buffer_len_ = 0;
    total_len_ = 0;
    finalized_ = false;
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
}

void Sha256::process_block(const u8* block) noexcept {
    u32 w[64];
    
    // Prepare message schedule
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<u32>(block[i * 4]) << 24) |
               (static_cast<u32>(block[i * 4 + 1]) << 16) |
               (static_cast<u32>(block[i * 4 + 2]) << 8) |
               static_cast<u32>(block[i * 4 + 3]);
    }
    
    for (int i = 16; i < 64; ++i) {
        w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
    }
    
    // Working variables
    u32 a = state_[0];
    u32 b = state_[1];
    u32 c = state_[2];
    u32 d = state_[3];
    u32 e = state_[4];
    u32 f = state_[5];
    u32 g = state_[6];
    u32 h = state_[7];
    
    // Main loop
    for (int i = 0; i < 64; ++i) {
        u32 t1 = h + sigma1(e) + choose(e, f, g) + K[i] + w[i];
        u32 t2 = sigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // Add to hash
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::array<char, 65> Sha256::to_hex(const Hash256& hash) noexcept {
    std::array<char, 65> hex;
    hex[64] = '\0';
    
    static constexpr char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex[i * 2] = hex_chars[hash.data[i] >> 4];
        hex[i * 2 + 1] = hex_chars[hash.data[i] & 0x0F];
    }
    
    return hex;
}

} // namespace vectortick
