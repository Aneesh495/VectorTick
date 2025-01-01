#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace vectortick {

// Byte order conversion utilities
// All wire formats use network byte order (big-endian)

namespace detail {

inline bool is_little_endian() noexcept {
    const u16 test = 0x0001;
    return *reinterpret_cast<const u8*>(&test) == 0x01;
}

inline bool is_big_endian() noexcept {
    return !is_little_endian();
}

} // namespace detail

// Convert from host to network byte order (big-endian)
inline u16 host_to_network(u16 value) noexcept {
    if (detail::is_little_endian()) {
        return static_cast<u16>((value >> 8) | (value << 8));
    }
    return value;
}

inline u32 host_to_network(u32 value) noexcept {
    if (detail::is_little_endian()) {
        return ((value >> 24) & 0x000000FFU) |
               ((value >>  8) & 0x0000FF00U) |
               ((value <<  8) & 0x00FF0000U) |
               ((value << 24) & 0xFF000000U);
    }
    return value;
}

inline u64 host_to_network(u64 value) noexcept {
    if (detail::is_little_endian()) {
        return ((value >> 56) & 0x00000000000000FFULL) |
               ((value >> 40) & 0x000000000000FF00ULL) |
               ((value >> 24) & 0x0000000000FF0000ULL) |
               ((value >>  8) & 0x00000000FF000000ULL) |
               ((value <<  8) & 0x000000FF00000000ULL) |
               ((value << 24) & 0x0000FF0000000000ULL) |
               ((value << 40) & 0x00FF000000000000ULL) |
               ((value << 56) & 0xFF00000000000000ULL);
    }
    return value;
}

inline i16 host_to_network(i16 value) noexcept {
    return static_cast<i16>(host_to_network(static_cast<u16>(value)));
}

inline i32 host_to_network(i32 value) noexcept {
    return static_cast<i32>(host_to_network(static_cast<u32>(value)));
}

inline i64 host_to_network(i64 value) noexcept {
    return static_cast<i64>(host_to_network(static_cast<u64>(value)));
}

// Convert from network to host byte order
template <typename T>
inline T network_to_host(T value) noexcept {
    return host_to_network(value);  // Same operation
}

// Read big-endian value from buffer (unaligned safe)
inline u16 read_be_u16(const byte* ptr) noexcept {
    u16 value;
    std::memcpy(&value, ptr, sizeof(value));
    return network_to_host(value);
}

inline u32 read_be_u32(const byte* ptr) noexcept {
    u32 value;
    std::memcpy(&value, ptr, sizeof(value));
    return network_to_host(value);
}

inline u64 read_be_u64(const byte* ptr) noexcept {
    u64 value;
    std::memcpy(&value, ptr, sizeof(value));
    return network_to_host(value);
}

inline i16 read_be_i16(const byte* ptr) noexcept {
    return static_cast<i16>(read_be_u16(ptr));
}

inline i32 read_be_i32(const byte* ptr) noexcept {
    return static_cast<i32>(read_be_u32(ptr));
}

inline i64 read_be_i64(const byte* ptr) noexcept {
    return static_cast<i64>(read_be_u64(ptr));
}

// Write big-endian value to buffer (unaligned safe)
inline void write_be_u16(byte* ptr, u16 value) noexcept {
    const u16 be_value = host_to_network(value);
    std::memcpy(ptr, &be_value, sizeof(be_value));
}

inline void write_be_u32(byte* ptr, u32 value) noexcept {
    const u32 be_value = host_to_network(value);
    std::memcpy(ptr, &be_value, sizeof(be_value));
}

inline void write_be_u64(byte* ptr, u64 value) noexcept {
    const u64 be_value = host_to_network(value);
    std::memcpy(ptr, &be_value, sizeof(be_value));
}

inline void write_be_i16(byte* ptr, i16 value) noexcept {
    write_be_u16(ptr, static_cast<u16>(value));
}

inline void write_be_i32(byte* ptr, i32 value) noexcept {
    write_be_u32(ptr, static_cast<u32>(value));
}

inline void write_be_i64(byte* ptr, i64 value) noexcept {
    write_be_u64(ptr, static_cast<u64>(value));
}

// Little-endian variants for PCAP
inline u16 read_le_u16(const byte* ptr) noexcept {
    u16 value;
    std::memcpy(&value, ptr, sizeof(value));
    if (detail::is_big_endian()) {
        return static_cast<u16>((value >> 8) | (value << 8));
    }
    return value;
}

inline u32 read_le_u32(const byte* ptr) noexcept {
    u32 value;
    std::memcpy(&value, ptr, sizeof(value));
    if (detail::is_big_endian()) {
        return ((value >> 24) & 0x000000FFU) |
               ((value >>  8) & 0x0000FF00U) |
               ((value <<  8) & 0x00FF0000U) |
               ((value << 24) & 0xFF000000U);
    }
    return value;
}

inline u64 read_le_u64(const byte* ptr) noexcept {
    u64 value;
    std::memcpy(&value, ptr, sizeof(value));
    if (detail::is_big_endian()) {
        return ((value >> 56) & 0x00000000000000FFULL) |
               ((value >> 40) & 0x000000000000FF00ULL) |
               ((value >> 24) & 0x0000000000FF0000ULL) |
               ((value >>  8) & 0x00000000FF000000ULL) |
               ((value <<  8) & 0x000000FF00000000ULL) |
               ((value << 24) & 0x0000FF0000000000ULL) |
               ((value << 40) & 0x00FF000000000000ULL) |
               ((value << 56) & 0xFF00000000000000ULL);
    }
    return value;
}

inline void write_le_u16(byte* ptr, u16 value) noexcept {
    u16 le_value = value;
    if (detail::is_big_endian()) {
        le_value = static_cast<u16>((value >> 8) | (value << 8));
    }
    std::memcpy(ptr, &le_value, sizeof(le_value));
}

inline void write_le_u32(byte* ptr, u32 value) noexcept {
    u32 le_value = value;
    if (detail::is_big_endian()) {
        le_value = ((value >> 24) & 0x000000FFU) |
                   ((value >>  8) & 0x0000FF00U) |
                   ((value <<  8) & 0x00FF0000U) |
                   ((value << 24) & 0xFF000000U);
    }
    std::memcpy(ptr, &le_value, sizeof(le_value));
}

inline void write_le_u64(byte* ptr, u64 value) noexcept {
    u64 le_value = value;
    if (detail::is_big_endian()) {
        le_value = ((value >> 56) & 0x00000000000000FFULL) |
                   ((value >> 40) & 0x000000000000FF00ULL) |
                   ((value >> 24) & 0x0000000000FF0000ULL) |
                   ((value >>  8) & 0x00000000FF000000ULL) |
                   ((value <<  8) & 0x000000FF00000000ULL) |
                   ((value << 24) & 0x0000FF0000000000ULL) |
                   ((value << 40) & 0x00FF000000000000ULL) |
                   ((value << 56) & 0xFF00000000000000ULL);
    }
    std::memcpy(ptr, &le_value, sizeof(le_value));
}

} // namespace vectortick
