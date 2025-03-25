#pragma once

#include "types.hpp"
#include "crc32c.hpp"
#include <array>
#include <cstring>

namespace vectortick {

// SHA-256 implementation for content-addressed storage
// Standard FIPS 180-4 implementation
class Sha256 {
public:
    // Compute SHA-256 hash of data
    [[nodiscard]] static Hash256 compute(const byte* data, usize length) noexcept;
    
    // Compute SHA-256 hash incrementally
    void update(const byte* data, usize length) noexcept;
    
    // Finalize and get the hash
    [[nodiscard]] Hash256 finalize() noexcept;
    
    // Reset to initial state
    void reset() noexcept;
    
    // Get hex string representation
    [[nodiscard]] static std::array<char, 65> to_hex(const Hash256& hash) noexcept;

private:
    void process_block(const u8* block) noexcept;
    
    alignas(64) u8 buffer_[64];
    usize buffer_len_ = 0;
    u64 total_len_ = 0;
    
    u32 state_[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    bool finalized_ = false;
};

// PCG32 random number generator for deterministic generation
// Based on PCG-XSH-RR variant
class Pcg32 {
public:
    explicit Pcg32(u64 seed = 0x853c49e6748fea9bULL, u64 stream = 0xda3e39cb94b95bdbULL) noexcept
        : state_(0), inc_((stream << 1) | 1) {
        step();
        state_ += seed;
        step();
    }
    
    // Generate next random value
    [[nodiscard]] u32 next() noexcept {
        const u64 old_state = state_;
        step();
        return static_cast<u32>(((old_state >> 18) ^ old_state) >> 27);
    }
    
    // Generate value in range [0, bound)
    [[nodiscard]] u32 next_bounded(u32 bound) noexcept {
        // Lemire's method for unbiased bounded generation
        u32 r = next();
        u64 m = static_cast<u64>(r) * bound;
        u32 l = static_cast<u32>(m);
        if (l < bound) {
            u32 t = -bound;
            if (t >= bound) {
                t -= bound;
                if (t >= bound) {
                    t %= bound;
                }
            }
            while (l < t) {
                r = next();
                m = static_cast<u64>(r) * bound;
                l = static_cast<u32>(m);
            }
        }
        return m >> 32;
    }
    
    // Get/set state for checkpointing
    [[nodiscard]] u64 state() const noexcept { return state_; }
    [[nodiscard]] u64 inc() const noexcept { return inc_; }
    void set_state(u64 state, u64 inc) noexcept {
        state_ = state;
        inc_ = inc;
    }
    
private:
    void step() noexcept {
        state_ = state_ * 6364136223846793005ULL + inc_;
    }
    
    u64 state_;
    u64 inc_;
};

// Xoroshiro128+ for faster generation when statistical quality is less critical
class Xoroshiro128Plus {
public:
    explicit Xoroshiro128Plus(u64 seed = 0) noexcept {
        // Initialize using SplitMix64
        u64 z = seed + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        s_[0] = z ^ (z >> 31);
        
        z = (z + 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        s_[1] = z ^ (z >> 31);
    }
    
    [[nodiscard]] u64 next() noexcept {
        const u64 s0 = s_[0];
        u64 s1 = s_[1];
        const u64 result = s0 + s1;
        
        s1 ^= s0;
        s_[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
        s_[1] = rotl(s1, 36);
        
        return result;
    }
    
    void jump() noexcept {
        // Advance state by 2^64 steps
        static constexpr u64 JUMP[] = { 0xBEAC0467EBA5FACBULL, 0xD86B048B86AA9922ULL };
        
        u64 s0 = 0, s1 = 0;
        for (usize i = 0; i < 2; ++i) {
            for (usize b = 0; b < 64; ++b) {
                if (JUMP[i] & (1ULL << b)) {
                    s0 ^= s_[0];
                    s1 ^= s_[1];
                }
                (void)next();
            }
        }
        s_[0] = s0;
        s_[1] = s1;
    }
    
private:
    static u64 rotl(u64 x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }
    
    u64 s_[2];
};

// Hash combinators
template <typename T>
inline u64 hash_value(const T& value) noexcept {
    return static_cast<u64>(std::hash<T>{}(value));
}

inline u64 hash_combine(u64 seed, u64 value) noexcept {
    // Boost hash combine formula
    return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

// 128-bit hash for content addressing
struct Hash128 {
    u64 low;
    u64 high;
    
    bool operator==(const Hash128& other) const noexcept {
        return low == other.low && high == other.high;
    }
    
    bool operator!=(const Hash128& other) const noexcept {
        return !(*this == other);
    }
    
    [[nodiscard]] u64 hash() const noexcept {
        return hash_combine(low, high);
    }
};

// Fast hash for small values (FNV-1a variant)
class FnvHash {
public:
    FnvHash() : hash_(14695981039346656037ULL) {}
    
    void update(const u8* data, usize length) noexcept {
        for (usize i = 0; i < length; ++i) {
            hash_ ^= data[i];
            hash_ *= 1099511628211ULL;
        }
    }
    
    [[nodiscard]] u64 get() const noexcept { return hash_; }
    void reset() noexcept { hash_ = 14695981039346656037ULL; }

private:
    u64 hash_;
};

} // namespace vectortick
