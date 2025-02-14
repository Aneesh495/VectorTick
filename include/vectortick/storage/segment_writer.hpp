#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "../common/crc32c.hpp"
#include "../common/hash.hpp"
#include "../memory/aligned_buffer.hpp"
#include "../model/event.hpp"
#include "file_format.hpp"
#include <string>
#include <vector>
#include <memory>

namespace vectortick {

// Segment writer - encodes events into VTS1 columnar format
class SegmentWriter {
public:
    explicit SegmentWriter(u64 segment_id = 0, usize max_rows = SegmentConfig::DefaultRowsPerSegment);
    ~SegmentWriter() = default;
    
    // Non-copyable
    SegmentWriter(const SegmentWriter&) = delete;
    SegmentWriter& operator=(const SegmentWriter&) = delete;
    
    // Add event to segment
    [[nodiscard]] Status add_event(const CanonicalEvent& event) noexcept;
    
    // Check if segment is full
    [[nodiscard]] bool is_full() const noexcept { return row_count_ >= max_rows_; }
    [[nodiscard]] bool is_empty() const noexcept { return row_count_ == 0; }
    
    // Get current row count
    [[nodiscard]] usize row_count() const noexcept { return row_count_; }
    
    // Get segment statistics
    [[nodiscard]] u64 min_timestamp() const noexcept { return min_timestamp_; }
    [[nodiscard]] u64 max_timestamp() const noexcept { return max_timestamp_; }
    [[nodiscard]] u64 min_sequence() const noexcept { return min_sequence_; }
    [[nodiscard]] u64 max_sequence() const noexcept { return max_sequence_; }
    
    // Finalize and write to file
    [[nodiscard]] Status write_to_file(const std::string& path) noexcept;
    
    // Reset for reuse
    void reset(u64 new_segment_id = 0) noexcept;
    
    // Get estimated size
    [[nodiscard]] usize estimated_size() const noexcept;

private:
    // Column buffers
    std::vector<u64> exchange_ts_ns_;
    std::vector<u64> receive_ts_ns_;
    std::vector<u64> sequences_;
    std::vector<u32> instrument_ids_;
    std::vector<u8> event_types_;
    std::vector<u8> sides_;
    std::vector<u16> flags_;
    std::vector<i64> price_ticks_;
    std::vector<u32> quantities_;
    std::vector<u16> venue_ids_;
    std::vector<u16> source_ids_;
    std::vector<u64> trade_or_order_ids_;
    
    // Statistics
    u64 segment_id_;
    usize max_rows_;
    usize row_count_;
    u64 min_timestamp_;
    u64 max_timestamp_;
    u64 min_sequence_;
    u64 max_sequence_;
    
    // Zone map data
    u64 min_instrument_id_;
    u64 max_instrument_id_;
    i64 min_price_;
    i64 max_price_;
    u32 min_quantity_;
    u32 max_quantity_;
    
    // Encoding helpers
    [[nodiscard]] Status encode_column_u64(const std::vector<u64>& values, 
                                            vts1::EncodingType encoding,
                                            byte* output, 
                                            usize& offset,
                                            usize output_size) noexcept;
    
    [[nodiscard]] Status encode_column_u32(const std::vector<u32>& values,
                                            vts1::EncodingType encoding,
                                            byte* output,
                                            usize& offset,
                                            usize output_size) noexcept;
    
    [[nodiscard]] Status encode_column_i64(const std::vector<i64>& values,
                                            vts1::EncodingType encoding,
                                            byte* output,
                                            usize& offset,
                                            usize output_size) noexcept;
    
    [[nodiscard]] Status encode_column_u16(const std::vector<u16>& values,
                                            vts1::EncodingType encoding,
                                            byte* output,
                                            usize& offset,
                                            usize output_size) noexcept;
    
    [[nodiscard]] Status encode_column_u8(const std::vector<u8>& values,
                                           vts1::EncodingType encoding,
                                           byte* output,
                                           usize& offset,
                                           usize output_size) noexcept;
    
    void update_statistics(const CanonicalEvent& event) noexcept;
    
    [[nodiscard]] usize select_encoding_size(u64 min_val, u64 max_val) const noexcept;
};

} // namespace vectortick
