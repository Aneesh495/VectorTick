#pragma once

#include "../common/types.hpp"
#include "../common/endian.hpp"
#include "../common/status.hpp"
#include "../common/crc32c.hpp"
#include <cstring>

namespace vectortick {

// VTP1 Wire Protocol Frame Format
// All integers are big-endian (network byte order)
// 
// Header (40 bytes):
//   magic              u32   0x56545031 ("VTP1")
//   version            u8    1
//   message_type       u8    
//   flags              u16   
//   payload_length     u32   
//   session_id         u32   
//   sequence           u64   
//   send_timestamp_ns  u64   
//   crc32c             u32   (covers header with CRC=0 + payload)
//   reserved           u32   must be zero
//
// Total header: 40 bytes

namespace vtp1 {

// Message types
enum class MessageType : u8 {
    StreamMetadata = 0,
    Quote = 1,
    Trade = 2,
    BookDelta = 3,
    Status = 4,
    Heartbeat = 5,
    EndOfStream = 255
};

// Message flags
struct MessageFlags {
    static constexpr u16 Compressed = 0x0001;
    static constexpr u16 Snapshot = 0x0002;
    static constexpr u16 LastInBatch = 0x0004;
};

// Frame header structure
struct alignas(8) FrameHeader {
    u32 magic;
    u8 version;
    u8 message_type;
    u16 flags;
    u32 payload_length;
    u32 session_id;
    u64 sequence;
    u64 send_timestamp_ns;
    u32 crc32c;
    u32 reserved;
    
    static constexpr usize Size = 40;
    
    [[nodiscard]] bool is_valid_magic() const noexcept {
        return magic == Magic::VTP1;
    }
    
    [[nodiscard]] bool is_valid_version() const noexcept {
        return version == Version::VTP1;
    }
    
    [[nodiscard]] bool is_valid_reserved() const noexcept {
        return reserved == 0;
    }
};

// Frame reader/writer
class Frame {
public:
    // Read frame header from buffer (big-endian)
    static void read_header(const byte* buffer, FrameHeader& header) noexcept {
        header.magic = read_be_u32(buffer);
        header.version = buffer[4];
        header.message_type = buffer[5];
        header.flags = read_be_u16(buffer + 6);
        header.payload_length = read_be_u32(buffer + 8);
        header.session_id = read_be_u32(buffer + 12);
        header.sequence = read_be_u64(buffer + 16);
        header.send_timestamp_ns = read_be_u64(buffer + 24);
        header.crc32c = read_be_u32(buffer + 32);
        header.reserved = read_be_u32(buffer + 36);
    }
    
    // Write frame header to buffer (big-endian)
    static void write_header(byte* buffer, const FrameHeader& header) noexcept {
        write_be_u32(buffer, header.magic);
        buffer[4] = header.version;
        buffer[5] = header.message_type;
        write_be_u16(buffer + 6, header.flags);
        write_be_u32(buffer + 8, header.payload_length);
        write_be_u32(buffer + 12, header.session_id);
        write_be_u64(buffer + 16, header.sequence);
        write_be_u64(buffer + 24, header.send_timestamp_ns);
        write_be_u32(buffer + 32, header.crc32c);
        write_be_u32(buffer + 36, header.reserved);
    }
    
    // Compute CRC for frame (header with CRC=0 + payload)
    [[nodiscard]] static u32 compute_crc(const FrameHeader& header, 
                                         const byte* payload) noexcept {
        // Create copy of header with CRC = 0
        FrameHeader temp = header;
        temp.crc32c = 0;
        
        // Write to temporary buffer
        alignas(8) byte header_buf[FrameHeader::Size];
        write_header(header_buf, temp);
        
        // Compute CRC
        u32 crc = Crc32C::compute(header_buf, FrameHeader::Size);
        if (header.payload_length > 0 && payload) {
            crc = Crc32C::compute(crc, payload, header.payload_length);
        }
        
        return crc;
    }
    
    // Validate frame
    [[nodiscard]] static Status validate(const FrameHeader& header,
                                         const byte* payload,
                                         usize available_bytes) noexcept {
        // Check magic
        if (!header.is_valid_magic()) {
            return Status(StatusCode::InvalidMagic, "Invalid VTP1 magic");
        }
        
        // Check version
        if (!header.is_valid_version()) {
            return Status(StatusCode::InvalidVersion, "Unsupported VTP1 version");
        }
        
        // Check reserved
        if (!header.is_valid_reserved()) {
            return Status(StatusCode::ReservedFieldNotZero, "Reserved field not zero");
        }
        
        // Check payload length
        if (header.payload_length > Limits::MaxPayloadLength) {
            return Status(StatusCode::InvalidLength, "Payload too large");
        }
        
        // Check available bytes
        if (available_bytes < FrameHeader::Size + header.payload_length) {
            return Status(StatusCode::TruncatedFrame, "Frame truncated");
        }
        
        // Validate message type
        if (header.message_type > static_cast<u8>(MessageType::Heartbeat) &&
            header.message_type != static_cast<u8>(MessageType::EndOfStream)) {
            return Status(StatusCode::InvalidMessageType, "Unknown message type");
        }
        
        // Verify CRC
        u32 computed_crc = compute_crc(header, payload);
        if (computed_crc != header.crc32c) {
            return Status(StatusCode::InvalidChecksum, "CRC32C mismatch");
        }
        
        return Status::OK();
    }
    
    // Read complete frame from buffer
    // Returns bytes consumed, or 0 if not enough data
    [[nodiscard]] static usize read_frame(const byte* buffer, 
                                          usize buffer_size,
                                          FrameHeader& header,
                                          const byte*& payload) noexcept {
        if (buffer_size < FrameHeader::Size) {
            return 0;
        }
        
        read_header(buffer, header);
        
        usize total_size = FrameHeader::Size + header.payload_length;
        if (buffer_size < total_size) {
            return 0;
        }
        
        payload = buffer + FrameHeader::Size;
        return total_size;
    }
    
    // Write complete frame to buffer
    static void write_frame(byte* buffer,
                           MessageType type,
                           u16 flags,
                           u32 session_id,
                           u64 sequence,
                           u64 timestamp,
                           const byte* payload,
                           u32 payload_length) noexcept {
        FrameHeader header;
        header.magic = Magic::VTP1;
        header.version = Version::VTP1;
        header.message_type = static_cast<u8>(type);
        header.flags = flags;
        header.payload_length = payload_length;
        header.session_id = session_id;
        header.sequence = sequence;
        header.send_timestamp_ns = timestamp;
        header.crc32c = 0;
        header.reserved = 0;
        
        // Compute CRC
        header.crc32c = compute_crc(header, payload);
        
        // Write header
        write_header(buffer, header);
        
        // Copy payload
        if (payload && payload_length > 0) {
            std::memcpy(buffer + FrameHeader::Size, payload, payload_length);
        }
    }
};

// Stream metadata message payload
struct StreamMetadata {
    u64 start_time_ns;
    u64 end_time_ns;
    u32 source_id;
    u32 instrument_count;
    u32 event_count_hint;
    u16 flags;
    u8 time_resolution;  // 0=ns, 1=us, 2=ms
    u8 reserved;
    
    static constexpr usize Size = 32;
    
    void read_from(const byte* buffer) noexcept {
        start_time_ns = read_be_u64(buffer);
        end_time_ns = read_be_u64(buffer + 8);
        source_id = read_be_u32(buffer + 16);
        instrument_count = read_be_u32(buffer + 20);
        event_count_hint = read_be_u32(buffer + 24);
        flags = read_be_u16(buffer + 28);
        time_resolution = buffer[30];
        reserved = buffer[31];
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, start_time_ns);
        write_be_u64(buffer + 8, end_time_ns);
        write_be_u32(buffer + 16, source_id);
        write_be_u32(buffer + 20, instrument_count);
        write_be_u32(buffer + 24, event_count_hint);
        write_be_u16(buffer + 28, flags);
        buffer[30] = time_resolution;
        buffer[31] = reserved;
    }
};

} // namespace vtp1

} // namespace vectortick
