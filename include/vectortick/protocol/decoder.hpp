#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "../model/event.hpp"
#include "frame.hpp"
#include "messages.hpp"

namespace vectortick {

namespace vtp1 {

// VTP1 decoder - converts VTP1 frames to canonical events
class Decoder {
public:
    Decoder() = default;
    
    // Decode a complete frame into canonical event
    // Returns bytes consumed or error
    [[nodiscard]] Result<usize> decode_frame(const byte* data, 
                                              usize size,
                                              CanonicalEvent& event,
                                              u64 receive_ts_ns = 0) noexcept;
    
    // Get last decoded session ID
    [[nodiscard]] u32 last_session_id() const noexcept { return last_session_id_; }
    
    // Get last sequence number (for monotonicity checking)
    [[nodiscard]] u64 last_sequence() const noexcept { return last_sequence_; }
    
    // Reset decoder state
    void reset() noexcept {
        last_session_id_ = 0;
        last_sequence_ = 0;
    }
    
    // Statistics
    [[nodiscard]] u64 frames_decoded() const noexcept { return frames_decoded_; }
    [[nodiscard]] u64 decode_errors() const noexcept { return decode_errors_; }

private:
    [[nodiscard]] Status decode_quote(const byte* payload, u32 len, CanonicalEvent& event) noexcept;
    [[nodiscard]] Status decode_trade(const byte* payload, u32 len, CanonicalEvent& event) noexcept;
    [[nodiscard]] Status decode_book_delta(const byte* payload, u32 len, CanonicalEvent& event) noexcept;
    [[nodiscard]] Status decode_status(const byte* payload, u32 len, CanonicalEvent& event) noexcept;
    [[nodiscard]] Status decode_heartbeat(const byte* payload, u32 len, CanonicalEvent& event) noexcept;
    
    u32 last_session_id_ = 0;
    u64 last_sequence_ = 0;
    u64 frames_decoded_ = 0;
    u64 decode_errors_ = 0;
};

// VTP1 encoder - converts canonical events to VTP1 frames
class Encoder {
public:
    Encoder() = default;
    
    // Encode event into buffer
    // Returns bytes written or error
    [[nodiscard]] Result<usize> encode_event(const CanonicalEvent& event,
                                             byte* buffer,
                                             usize buffer_size,
                                             u32 session_id = 0) noexcept;
    
    // Encode stream metadata
    [[nodiscard]] Result<usize> encode_metadata(const StreamMetadata& metadata,
                                                byte* buffer,
                                                usize buffer_size,
                                                u32 session_id,
                                                u64 sequence) noexcept;
    
    // Encode end-of-stream marker
    [[nodiscard]] Result<usize> encode_end_of_stream(byte* buffer,
                                                      usize buffer_size,
                                                      u32 session_id,
                                                      u64 sequence,
                                                      u64 timestamp) noexcept;
    
    // Statistics
    [[nodiscard]] u64 frames_encoded() const noexcept { return frames_encoded_; }
    [[nodiscard]] u64 encode_errors() const noexcept { return encode_errors_; }

private:
    u64 frames_encoded_ = 0;
    u64 encode_errors_ = 0;
};

} // namespace vtp1

} // namespace vectortick
