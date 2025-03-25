#include "vectortick/common/crc32c.hpp"

#include "vectortick/common/crc32c.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>
#include <nmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_acle.h>
#include <sys/sysctl.h>
#endif

namespace vectortick {

// CRC32C polynomial: 0x1EDC6F41 (Castagnoli)
// Reflected polynomial: 0x82F63B78

// Lookup table for software implementation
alignas(64) u32 Crc32C::table_[256];
bool Crc32C::initialized_ = false;

// Initialize lookup table
void Crc32C::init() noexcept {
    if (initialized_) return;
    
    // CRC32C polynomial (reflected form)
    constexpr u32 polynomial = 0x82F63B78;
    
    for (u32 i = 0; i < 256; ++i) {
        u32 crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        table_[i] = crc;
    }
    
    initialized_ = true;
}

// Check hardware support
bool Crc32C::is_hw_supported() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    // Check for SSE4.2 via CPUID
    u32 eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return (ecx & bit_SSE4_2) != 0;
    }
    return false;
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARMv8 always has CRC instructions
    return true;
#else
    return false;
#endif
}

// Software implementation
u32 Crc32C::compute_sw(u32 init_crc, const byte* data, usize length) noexcept {
    if (!initialized_) init();
    
    u32 crc = init_crc;
    
    // Process byte by byte
    const u8* ptr = reinterpret_cast<const u8*>(data);
    const u8* end = ptr + length;
    while (ptr < end) {
        crc = table_[(crc ^ *ptr) & 0xFF] ^ (crc >> 8);
        ++ptr;
    }
    
    return crc;
}

// SSE4.2 implementation
#if defined(__x86_64__) || defined(_M_X64)
u32 Crc32C::compute_sse42(u32 init_crc, const byte* data, usize length) noexcept {
    u32 crc = init_crc;
    const u8* ptr = reinterpret_cast<const u8*>(data);
    
    // Process 8 bytes at a time
    while (length >= 8) {
        u64 val;
        __builtin_memcpy(&val, ptr, 8);
        crc = static_cast<u32>(_mm_crc32_u64(crc, val));
        ptr += 8;
        length -= 8;
    }
    
    // Process 4 bytes
    if (length >= 4) {
        u32 val;
        __builtin_memcpy(&val, ptr, 4);
        crc = _mm_crc32_u32(crc, val);
        ptr += 4;
        length -= 4;
    }
    
    // Process remaining bytes
    if (length >= 2) {
        u16 val;
        __builtin_memcpy(&val, ptr, 2);
        crc = _mm_crc32_u16(crc, val);
        ptr += 2;
        length -= 2;
    }
    
    if (length >= 1) {
        crc = _mm_crc32_u8(crc, *ptr);
    }
    
    return crc;
}
#endif

// ARM CRC implementation
#if defined(__aarch64__) || defined(_M_ARM64)
u32 Crc32C::compute_arm(u32 init_crc, const byte* data, usize length) noexcept {
    u32 crc = init_crc;
    const u8* ptr = reinterpret_cast<const u8*>(data);
    
    // Process 8 bytes at a time using CRC32CX
    while (length >= 8) {
        u64 val;
        __builtin_memcpy(&val, ptr, 8);
        crc = __crc32cd(crc, val);
        ptr += 8;
        length -= 8;
    }
    
    // Process 4 bytes using CRC32CW
    if (length >= 4) {
        u32 val;
        __builtin_memcpy(&val, ptr, 4);
        crc = __crc32cw(crc, val);
        ptr += 4;
        length -= 4;
    }
    
    // Process 2 bytes using CRC32CH
    if (length >= 2) {
        u16 val;
        __builtin_memcpy(&val, ptr, 2);
        crc = __crc32ch(crc, val);
        ptr += 2;
        length -= 2;
    }
    
    // Process remaining byte
    if (length >= 1) {
        crc = __crc32cb(crc, *ptr);
    }
    
    return crc;
}
#endif

// Main compute function (dispatches to best implementation)
u32 Crc32C::compute(u32 init_crc, const byte* data, usize length) noexcept {
    // Use hardware if available
    if (is_hw_supported()) {
        return compute_hw(data, length);
    }
    
    // Fallback to software
    return compute_sw(init_crc, data, length);
}

u32 Crc32C::compute(const byte* data, usize length) noexcept {
    constexpr u32 init_crc = 0xFFFFFFFF;
    return compute(init_crc, data, length) ^ 0xFFFFFFFF;
}

u32 Crc32C::compute_hw(const byte* data, usize length) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    if (is_hw_supported()) {
        constexpr u32 init_crc = 0;
        u32 crc = compute_sse42(init_crc, data, length);
        return crc ^ 0xFFFFFFFF;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    constexpr u32 init_crc = 0;
    u32 crc = compute_arm(init_crc, data, length);
    return crc ^ 0xFFFFFFFF;
#endif
    
    return compute_sw(0xFFFFFFFF, data, length);
}

// Combine two CRC values
// This allows computing CRC(A||B) from CRC(A) and CRC(B)
u32 Crc32C::combine(u32 crc_a, u32 crc_b, usize len_b) noexcept {
    if (len_b == 0) return crc_a;
    
    // GF(2) matrix multiplication approach
    // This is a simplified version; for production use, precompute powers
    
    // The math: CRC(A||B) = CRC_combine(CRC(A), CRC(B), len(B))
    // We need to "undo" the CRC of zero bytes and apply crc_b
    
    // Simplified approach: multiply in GF(2^32)
    // This uses the property that CRC is linear over GF(2)
    
    u32 result = crc_a;
    
    // Each zero byte "shifts" the CRC
    for (usize i = 0; i < len_b; ++i) {
        result = (result >> 8) ^ table_[result & 0xFF];
    }
    
    // XOR in crc_b (treating it as if it was computed from init=0)
    result ^= crc_b;
    
    return result;
}

} // namespace vectortick
