#include "vectortick/storage/segment_reader.hpp"
#include "vectortick/codec/bitpack.hpp"
#include "vectortick/codec/varint.hpp"
#include <cstring>

namespace vectortick {

Status SegmentReader::open(const std::string& path) noexcept {
    auto result = MappedFile::open_read(path);
    if (!result.ok()) {
        return result.status();
    }
    
    mapping_ = std::move(result.value());
    file_size_ = mapping_.size();
    
    auto status = read_header();
    if (!status.ok()) return status;
    
    status = read_descriptors();
    if (!status.ok()) return status;
    
    return Status::OK();
}

void SegmentReader::close() noexcept {
    mapping_.close();
    descriptors_.clear();
    zone_maps_.clear();
    bloom_filter_.clear();
    row_count_ = 0;
    segment_id_ = 0;
}

Status SegmentReader::read_header() noexcept {
    if (!mapping_.is_open()) {
        return Status(StatusCode::InvalidMapping, "No file mapped");
    }
    
    if (mapping_.size() < vts1::SegmentHeader::Size) {
        return Status(StatusCode::SegmentInvalidHeader, "File too small for header");
    }
    
    const byte* data = mapping_.data();
    std::memcpy(&header_, data, vts1::SegmentHeader::Size);
    
    // Validate magic
    if (!header_.is_valid_magic()) {
        return Status(StatusCode::SegmentInvalidHeader, "Invalid VTS1 magic");
    }
    
    // Validate version
    if (!header_.is_valid_version()) {
        return Status(StatusCode::SegmentInvalidHeader, "Unsupported VTS1 version");
    }
    
    // Validate CRC
    vts1::SegmentHeader temp = header_;
    temp.header_crc = 0;
    u32 computed_crc = Crc32C::compute(reinterpret_cast<byte*>(&temp), vts1::SegmentHeader::Size);
    
    if (computed_crc != header_.header_crc) {
        return Status(StatusCode::SegmentChecksumFailed, "Header CRC mismatch");
    }
    
    segment_id_ = header_.segment_id;
    row_count_ = static_cast<usize>(header_.row_count);
    min_timestamp_ = header_.min_timestamp;
    max_timestamp_ = header_.max_timestamp;
    
    return Status::OK();
}

Status SegmentReader::read_descriptors() noexcept {
    const byte* data = mapping_.data();
    
    // Read column descriptors
    usize num_columns = vts1::ColumnCount;
    descriptors_.resize(num_columns);
    
    usize desc_offset = header_.descriptor_offset;
    for (usize i = 0; i < num_columns; ++i) {
        if (desc_offset + vts1::ColumnDescriptor::Size > file_size_) {
            return Status(StatusCode::SegmentInvalidBlock, "Descriptor out of bounds");
        }
        
        std::memcpy(&descriptors_[i], data + desc_offset, vts1::ColumnDescriptor::Size);
        desc_offset += vts1::ColumnDescriptor::Size;
        
        // Validate descriptor CRC
        vts1::ColumnDescriptor temp = descriptors_[i];
        temp.descriptor_crc = 0;
        u32 computed_crc = Crc32C::compute(reinterpret_cast<byte*>(&temp),
                                           vts1::ColumnDescriptor::Size - sizeof(u32));
        
        if (computed_crc != descriptors_[i].descriptor_crc) {
            return Status(StatusCode::SegmentChecksumFailed, "Descriptor CRC mismatch");
        }
    }
    
    // Read zone maps
    usize zone_offset = header_.zone_map_offset;
    zone_maps_.resize(num_columns);
    
    for (usize i = 0; i < num_columns; ++i) {
        if (zone_offset + vts1::ZoneMap::Size > file_size_) {
            return Status(StatusCode::SegmentInvalidBlock, "Zone map out of bounds");
        }
        
        std::memcpy(&zone_maps_[i], data + zone_offset, vts1::ZoneMap::Size);
        zone_offset += vts1::ZoneMap::Size;
    }
    
    // Read bloom filter
    if (header_.bloom_filter_size > 0) {
        usize bf_offset = header_.bloom_filter_offset;
        if (bf_offset + header_.bloom_filter_size > file_size_) {
            return Status(StatusCode::SegmentInvalidBlock, "Bloom filter out of bounds");
        }
        
        bloom_filter_.resize(header_.bloom_filter_size);
        std::memcpy(bloom_filter_.data(), data + bf_offset, header_.bloom_filter_size);
    }
    
    return Status::OK();
}

Status SegmentReader::read_event(usize row_idx, CanonicalEvent& event) const noexcept {
    if (row_idx >= row_count_) {
        return Status(StatusCode::OutOfRange, "Row index out of range");
    }
    
    // For now, read each column individually
    u64 val_u64;
    u32 val_u32;
    i64 val_i64;
    u16 val_u16;
    u8 val_u8;
    
    // exchange_ts_ns
    auto status = read_column_u64(vts1::ColumnID::ExchangeTsNs, &val_u64, 1);
    if (!status.ok()) return status;
    event.exchange_ts_ns = val_u64;
    
    // receive_ts_ns
    status = read_column_u64(vts1::ColumnID::ReceiveTsNs, &val_u64, 1);
    if (!status.ok()) return status;
    event.receive_ts_ns = val_u64;
    
    // sequence
    status = read_column_u64(vts1::ColumnID::Sequence, &val_u64, 1);
    if (!status.ok()) return status;
    event.sequence = val_u64;
    
    // instrument_id
    status = read_column_u32(vts1::ColumnID::InstrumentId, &val_u32, 1);
    if (!status.ok()) return status;
    event.instrument_id = val_u32;
    
    // event_type
    status = read_column_u8(vts1::ColumnID::EventType, &val_u8, 1);
    if (!status.ok()) return status;
    event.event_type = static_cast<EventType>(val_u8);
    
    // side
    status = read_column_u8(vts1::ColumnID::Side, &val_u8, 1);
    if (!status.ok()) return status;
    event.side = static_cast<Side>(val_u8);
    
    // flags
    status = read_column_u16(vts1::ColumnID::Flags, &val_u16, 1);
    if (!status.ok()) return status;
    event.flags = val_u16;
    
    // price_ticks
    status = read_column_i64(vts1::ColumnID::PriceTicks, &val_i64, 1);
    if (!status.ok()) return status;
    event.price_ticks = val_i64;
    
    // quantity
    status = read_column_u32(vts1::ColumnID::Quantity, &val_u32, 1);
    if (!status.ok()) return status;
    event.quantity = val_u32;
    
    // venue_id
    status = read_column_u16(vts1::ColumnID::VenueId, &val_u16, 1);
    if (!status.ok()) return status;
    event.venue_id = val_u16;
    
    // source_id
    status = read_column_u16(vts1::ColumnID::SourceId, &val_u16, 1);
    if (!status.ok()) return status;
    event.source_id = val_u16;
    
    // trade_or_order_id
    status = read_column_u64(vts1::ColumnID::TradeOrOrderId, &val_u64, 1);
    if (!status.ok()) return status;
    event.trade_or_order_id = val_u64;
    
    return Status::OK();
}

Status SegmentReader::decode_column(const vts1::ColumnDescriptor& desc,
                                     byte* output,
                                     usize num_values) const noexcept {
    const byte* data = mapping_.data();
    const byte* col_data = data + desc.offset;
    
    // Validate data CRC
    u32 computed_crc = Crc32C::compute(col_data, desc.compressed_size);
    if (computed_crc != desc.data_crc) {
        return Status(StatusCode::SegmentChecksumFailed, "Column data CRC mismatch");
    }
    
    // Decode based on encoding type
    if (desc.encoding == vts1::EncodingType::Raw) {
        std::memcpy(output, col_data, desc.compressed_size);
        return Status::OK();
    }
    
    if (desc.encoding == vts1::EncodingType::Delta) {
        // Read first value
        std::memcpy(output, col_data, sizeof(u64));
        usize offset = sizeof(u64);
        
        u64* values = reinterpret_cast<u64*>(output);
        
        // Read deltas
        for (usize i = 1; i < num_values; ++i) {
            u64 delta;
            usize consumed = codec::VarIntU::decode(col_data + offset, desc.compressed_size - offset, delta);
            if (consumed == 0) {
                return Status(StatusCode::SegmentCorrupted, "Failed to decode delta");
            }
            offset += consumed;
            values[i] = values[i-1] + delta;
        }
        
        return Status::OK();
    }
    
    if (desc.encoding == vts1::EncodingType::BitPacked) {
        // Read reference value
        u32 reference;
        std::memcpy(&reference, col_data, sizeof(u32));
        usize offset = sizeof(u32);
        
        // Read bits per value
        u32 bits = static_cast<u32>(col_data[offset++]);
        
        // Decode packed values
        std::vector<u32> packed(num_values);
        usize consumed = codec::BitPackU32::decode(col_data + offset, desc.compressed_size - offset,
                                                    bits, packed.data(), num_values);
        if (consumed == 0) {
            return Status(StatusCode::SegmentCorrupted, "Failed to decode bit-packed values");
        }
        
        // Add reference back
        u32* values = reinterpret_cast<u32*>(output);
        for (usize i = 0; i < num_values; ++i) {
            values[i] = packed[i] + reference;
        }
        
        return Status::OK();
    }
    
    // Default: treat as raw
    std::memcpy(output, col_data, desc.compressed_size);
    return Status::OK();
}

Status SegmentReader::read_column_u64(vts1::ColumnID col, u64* values, usize num_values) const noexcept {
    if (col >= descriptors_.size()) {
        return Status(StatusCode::OutOfRange, "Invalid column ID");
    }
    
    return decode_column(descriptors_[col], reinterpret_cast<byte*>(values), num_values);
}

Status SegmentReader::read_column_u32(vts1::ColumnID col, u32* values, usize num_values) const noexcept {
    if (col >= descriptors_.size()) {
        return Status(StatusCode::OutOfRange, "Invalid column ID");
    }
    
    return decode_column(descriptors_[col], reinterpret_cast<byte*>(values), num_values);
}

Status SegmentReader::read_column_i64(vts1::ColumnID col, i64* values, usize num_values) const noexcept {
    if (col >= descriptors_.size()) {
        return Status(StatusCode::OutOfRange, "Invalid column ID");
    }
    
    return decode_column(descriptors_[col], reinterpret_cast<byte*>(values), num_values);
}

Status SegmentReader::read_column_u16(vts1::ColumnID col, u16* values, usize num_values) const noexcept {
    if (col >= descriptors_.size()) {
        return Status(StatusCode::OutOfRange, "Invalid column ID");
    }
    
    return decode_column(descriptors_[col], reinterpret_cast<byte*>(values), num_values);
}

Status SegmentReader::read_column_u8(vts1::ColumnID col, u8* values, usize num_values) const noexcept {
    if (col >= descriptors_.size()) {
        return Status(StatusCode::OutOfRange, "Invalid column ID");
    }
    
    return decode_column(descriptors_[col], reinterpret_cast<byte*>(values), num_values);
}

u64 SegmentReader::column_min(vts1::ColumnID col) const noexcept {
    if (col < descriptors_.size()) {
        return descriptors_[col].min_value;
    }
    return 0;
}

u64 SegmentReader::column_max(vts1::ColumnID col) const noexcept {
    if (col < descriptors_.size()) {
        return descriptors_[col].max_value;
    }
    return 0;
}

bool SegmentReader::might_contain_instrument(u32 instrument_id) const noexcept {
    if (bloom_filter_.empty()) {
        return true;  // No bloom filter, assume might contain
    }
    
    // Hash and check bits
    u64 hash = static_cast<u64>(instrument_id) * 14695981039346656037ULL;
    usize num_bits = bloom_filter_.size() * 8;
    
    for (usize h = 0; h < 7; ++h) {  // 7 hash functions
        usize bit_idx = (hash + h * h) % num_bits;
        if ((bloom_filter_[bit_idx / 8] & (1 << (bit_idx % 8))) == 0) {
            return false;
        }
    }
    
    return true;
}

Status SegmentReader::validate() const noexcept {
    // Already validated during open
    if (!is_open()) {
        return Status(StatusCode::InvalidMapping, "No segment open");
    }
    
    // Validate footer if exists
    if (header_.footer_offset + sizeof(vts1::SegmentFooter) <= file_size_) {
        vts1::SegmentFooter footer;
        const byte* data = mapping_.data();
        std::memcpy(&footer, data + header_.footer_offset, sizeof(vts1::SegmentFooter));
        
        if (footer.commit_marker != vts1::SegmentFooter::CommittedMarker) {
            return Status(StatusCode::SegmentCorrupted, "Segment not committed");
        }
        
        // Validate footer CRC
        vts1::SegmentFooter temp = footer;
        temp.footer_crc = 0;
        u32 computed_crc = Crc32C::compute(reinterpret_cast<byte*>(&temp),
                                           sizeof(vts1::SegmentFooter) - sizeof(u32));
        
        if (computed_crc != footer.footer_crc) {
            return Status(StatusCode::SegmentChecksumFailed, "Footer CRC mismatch");
        }
    }
    
    return Status::OK();
}

double SegmentReader::compression_ratio() const noexcept {
    if (row_count_ == 0) return 0.0;
    
    usize canonical_size = row_count_ * CanonicalEvent::LogicalSize;
    if (file_size_ == 0) return 0.0;
    
    return static_cast<double>(canonical_size) / static_cast<double>(file_size_);
}

} // namespace vectortick
