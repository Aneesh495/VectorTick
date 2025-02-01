#pragma once

#include "../common/types.hpp"
#include "../common/endian.hpp"
#include "../common/crc32c.hpp"
#include "../common/hash.hpp"
#include "../model/event.hpp"
#include <cstring>

namespace vectortick {

// VTS1 Storage Format
// Versioned, checksummed, mmap-readable columnar format
//
// Segment Layout:
//   Segment Header (fixed)
//   Column Descriptor Table
//   Column Blocks (one per column)
//   Zone Maps
//   Bloom Filter (instrument_id)
//   Segment Footer

namespace vts1 {

// Segment header (128 bytes)
struct SegmentHeader {
    u32 magic;              // VTS1 magic
    u32 version;            // Format version
    u64 schema_hash;        // Hash of schema definition
    u64 segment_id;         // Unique segment ID
    u64 row_count;          // Number of rows in segment
    u64 min_timestamp;      // Min exchange_ts_ns
    u64 max_timestamp;      // Max exchange_ts_ns
    u32 descriptor_offset;  // Offset to column descriptors
    u32 descriptor_size;    // Size of descriptor table
    u32 zone_map_offset;    // Offset to zone maps
    u32 bloom_filter_offset;// Offset to bloom filter
    u32 bloom_filter_size;  // Size of bloom filter
    u32 footer_offset;      // Offset to footer
    u32 reserved[8];        // Reserved for future use
    u32 header_crc;         // CRC32C of header (with this field = 0)
    
    static constexpr usize Size = 128;
    
    [[nodiscard]] bool is_valid_magic() const noexcept { return magic == Magic::VTS1; }
    [[nodiscard]] bool is_valid_version() const noexcept { return version == Version::VTS1; }
};

// Column types
enum class ColumnType : u8 {
    U64 = 0,
    I64 = 1,
    U32 = 2,
    U16 = 3,
    U8 = 4,
    I32 = 5,
    I16 = 6,
    I8 = 7,
    Bool = 8
};

// Encoding types
enum class EncodingType : u8 {
    Raw = 0,            // Uncompressed fixed-width
    VarInt = 1,         // Variable-length integer
    BitPacked = 2,      // Frame-of-reference bit packing
    Delta = 3,          // Delta encoding
    DeltaOfDelta = 4,   // Delta-of-delta for timestamps
    RLE = 5,            // Run-length encoding
    Dictionary = 6      // Dictionary encoding
};

// Column descriptor (64 bytes)
struct ColumnDescriptor {
    u32 column_id;          // Column index
    ColumnType type;        // Data type
    EncodingType encoding;  // Encoding used
    u8 bits_per_value;      // For bit-packed encoding
    u8 reserved[5];
    u64 offset;             // Offset to column data
    u64 compressed_size;    // Compressed size in bytes
    u64 uncompressed_size;  // Uncompressed size in bytes (logical)
    u64 min_value;          // Minimum value (as u64)
    u64 max_value;          // Maximum value (as u64)
    u32 null_count;         // Number of nulls (always 0 for now)
    u32 data_crc;           // CRC32C of column data
    u32 descriptor_crc;     // CRC32C of descriptor (with this field = 0)
    
    static constexpr usize Size = 64;
};

// Zone map for a column (64 bytes)
struct ZoneMap {
    u64 min_value;
    u64 max_value;
    u64 sum_value;      // For approximate aggregates
    u32 row_count;
    u32 null_count;
    u32 first_row_idx;  // Index of first row in zone
    u32 last_row_idx;   // Index of last row in zone
    u32 reserved[4];
    
    static constexpr usize Size = 64;
};

// Segment footer (128 bytes)
struct SegmentFooter {
    u32 magic;                  // VTS1 magic
    u32 version;                // Format version
    u64 total_size;             // Total file size
    u64 descriptor_table_hash;  // Hash of descriptor table
    u64 segment_digest;         // Hash of all column data
    u64 commit_marker;          // Non-zero if committed
    u64 create_timestamp;       // Creation timestamp
    u64 min_sequence;           // Min sequence number
    u64 max_sequence;           // Max sequence number
    u32 row_count;              // Number of rows
    u32 column_count;           // Number of columns
    u32 reserved[6];
    u32 footer_crc;             // CRC32C of footer (with this field = 0)
    
    static constexpr usize Size = 128;
    static constexpr u64 CommittedMarker = 0xDEADBEEFCAFEBABEULL;
};

// Bloom filter parameters
struct BloomFilterParams {
    static constexpr usize DefaultBitsPerItem = 10;
    static constexpr usize DefaultNumHashes = 7;
    
    usize num_bits;
    usize num_hashes;
    usize size_bytes;
    
    static BloomFilterParams for_items(usize num_items, usize bits_per_item = DefaultBitsPerItem) noexcept {
        BloomFilterParams params;
        params.num_bits = num_items * bits_per_item;
        // Round up to power of 2
        usize bits = 1;
        while (bits < params.num_bits) bits <<= 1;
        params.num_bits = bits;
        params.num_hashes = DefaultNumHashes;
        params.size_bytes = params.num_bits / 8;
        return params;
    }
};

// Column ID enumeration for canonical schema
enum ColumnID : u32 {
    ExchangeTsNs = 0,
    ReceiveTsNs = 1,
    Sequence = 2,
    InstrumentId = 3,
    EventType = 4,
    Side = 5,
    Flags = 6,
    PriceTicks = 7,
    Quantity = 8,
    VenueId = 9,
    SourceId = 10,
    TradeOrOrderId = 11,
    ColumnCount = 12
};

// Get column type for each column
[[nodiscard]] inline ColumnType get_column_type(ColumnID id) noexcept {
    switch (id) {
        case ExchangeTsNs:
        case ReceiveTsNs:
        case Sequence:
        case TradeOrOrderId:
            return ColumnType::U64;
        case InstrumentId:
        case Quantity:
            return ColumnType::U32;
        case EventType:
        case Side:
            return ColumnType::U8;
        case Flags:
        case VenueId:
        case SourceId:
            return ColumnType::U16;
        case PriceTicks:
            return ColumnType::I64;
        default:
            return ColumnType::U64;
    }
}

// Get column name
[[nodiscard]] inline const char* get_column_name(ColumnID id) noexcept {
    switch (id) {
        case ExchangeTsNs: return "exchange_ts_ns";
        case ReceiveTsNs: return "receive_ts_ns";
        case Sequence: return "sequence";
        case InstrumentId: return "instrument_id";
        case EventType: return "event_type";
        case Side: return "side";
        case Flags: return "flags";
        case PriceTicks: return "price_ticks";
        case Quantity: return "quantity";
        case VenueId: return "venue_id";
        case SourceId: return "source_id";
        case TradeOrOrderId: return "trade_or_order_id";
        default: return "unknown";
    }
}

} // namespace vts1

} // namespace vectortick
