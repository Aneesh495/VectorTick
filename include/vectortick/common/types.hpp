#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace vectortick {

// Fixed-width integer types
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// Byte type for raw memory operations
// Using u8 for better compatibility with standard library functions
using byte = u8;

// Strong type for hash values
struct Hash256 {
    u8 data[32];
    
    bool operator==(const Hash256& other) const noexcept {
        for (usize i = 0; i < 32; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
    
    bool operator!=(const Hash256& other) const noexcept {
        return !(*this == other);
    }
};

// Event types (canonical schema)
enum class EventType : u8 {
    Quote = 0,
    Trade = 1,
    BookDelta = 2,
    Status = 3,
    Heartbeat = 4,
    Invalid = 255
};

// Order side
enum class Side : u8 {
    Bid = 0,
    Ask = 1,
    Invalid = 255
};

// Event flags
struct EventFlags {
    u16 value;
    
    static constexpr u16 LastInSequence = 0x0001;
    static constexpr u16 GapInSequence = 0x0002;
    static constexpr u16 OutOfOrder = 0x0004;
    static constexpr u16 Snapshot = 0x0008;
};

// Canonical event (56 bytes logical schema, not packed)
// Prices are integer ticks, quantities are integer lots
// Timestamps are integer nanoseconds since Unix epoch
struct CanonicalEvent {
    u64 exchange_ts_ns;    // 8 bytes - exchange timestamp
    u64 receive_ts_ns;     // 8 bytes - receive timestamp
    u64 sequence;          // 8 bytes - source sequence number
    u32 instrument_id;     // 4 bytes - instrument identifier
    EventType event_type;  // 1 byte  - event type enum
    Side side;             // 1 byte  - bid/ask side
    u16 flags;             // 2 bytes - event flags
    i64 price_ticks;       // 8 bytes - price in ticks (signed for spread calculations)
    u32 quantity;          // 4 bytes - quantity in lots
    u16 venue_id;          // 2 bytes - venue identifier
    u16 source_id;         // 2 bytes - source identifier
    u64 trade_or_order_id; // 8 bytes - trade or order identifier
    
    // Total logical size: 56 bytes
    static constexpr usize LogicalSize = 56;
    
    // Basic validation
    [[nodiscard]] bool is_valid() const noexcept {
        if (event_type == EventType::Invalid) return false;
        if (side == Side::Invalid && event_type != EventType::Heartbeat) return false;
        return true;
    }
};

// Segment configuration
struct SegmentConfig {
    static constexpr usize DefaultRowsPerSegment = 65536;
    static constexpr usize MaxRowsPerSegment = 1048576;
    static constexpr usize BatchSize = 1024;
    static constexpr usize Alignment = 64;
};

// Magic numbers
struct Magic {
    static constexpr u32 VTP1 = 0x56545031;  // "VTP1"
    static constexpr u32 VTS1 = 0x56545331;  // "VTS1"
};

// Version numbers
struct Version {
    static constexpr u8 VTP1 = 1;
    static constexpr u8 VTS1 = 1;
};

// Limits
struct Limits {
    static constexpr usize MaxPayloadLength = 65535;
    static constexpr usize MaxInstruments = 16384;
    static constexpr usize MaxVenues = 256;
    static constexpr usize MaxSources = 256;
    static constexpr u64 MaxTimestamp = 2534023007990000000ULL; // Year 2050
    static constexpr i64 MaxPrice = 1000000000000LL;
    static constexpr u32 MaxQuantity = 1000000000U;
};

} // namespace vectortick
