#pragma once

#include "types.hpp"
#include "status.hpp"

namespace vectortick {

// CRC32C implementation with hardware acceleration
// Castagnoli polynomial (0x1EDC6F41) used by iSCSI and many storage systems

class Crc32C {
public:
    // Initialize CRC table (must be called before use)
    static void init() noexcept;
    
    // Compute CRC32C of a buffer (software fallback)
    [[nodiscard]] static u32 compute(const byte* data, usize length) noexcept;
    
    // Compute CRC32C with initial value (for incremental updates)
    [[nodiscard]] static u32 compute(u32 init_crc, const byte* data, usize length) noexcept;
    
    // Hardware-accelerated version if available, otherwise falls back to software
    [[nodiscard]] static u32 compute_hw(const byte* data, usize length) noexcept;
    
    // Check if hardware acceleration is available
    [[nodiscard]] static bool is_hw_supported() noexcept;
    
    // Combine two CRCs (CRC(A || B) = combine(CRC(A), CRC(B), len(B)))
    [[nodiscard]] static u32 combine(u32 crc_a, u32 crc_b, usize len_b) noexcept;
    
private:
    // Software table-based implementation
    [[nodiscard]] static u32 compute_sw(u32 init_crc, const byte* data, usize length) noexcept;
    
    // SSE4.2 hardware implementation (x86-64)
    [[nodiscard]] static u32 compute_sse42(u32 init_crc, const byte* data, usize length) noexcept;
    
    // ARM CRC hardware implementation (AArch64)
    [[nodiscard]] static u32 compute_arm(u32 init_crc, const byte* data, usize length) noexcept;
    
    // Precomputed lookup table for software implementation
    static u32 table_[256];
    static bool initialized_;
};

// Convenience function
inline u32 crc32c(const byte* data, usize length) noexcept {
    return Crc32C::compute(data, length);
}

inline u32 crc32c(u32 init_crc, const byte* data, usize length) noexcept {
    return Crc32C::compute(init_crc, data, length);
}

} // namespace vectortick
