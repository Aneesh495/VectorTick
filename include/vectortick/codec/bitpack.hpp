#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "../common/checked_math.hpp"
#include <cstring>

namespace vectortick {

// Bit-packing codecs for columnar compression
// Frame-of-reference (FOR) integer compression

namespace codec {

// Bit-packed unsigned 32-bit integers
// Packs N values using exactly bits_per_value bits each
class BitPackU32 {
public:
    // Calculate required bits to store max_value
    [[nodiscard]] static u32 bits_required(u32 max_value) noexcept {
        if (max_value == 0) return 1;
        u32 bits = 0;
        while (max_value > 0) {
            bits++;
            max_value >>= 1;
        }
        return bits;
    }
    
    // Calculate max encoded size
    [[nodiscard]] static usize max_encoded_size(usize num_values, u32 bits) noexcept {
        if (bits == 0) return 0;
        usize total_bits = num_values * bits;
        return (total_bits + 7) / 8;
    }
    
    // Encode values using bit-packing
    // Returns encoded size
    [[nodiscard]] static usize encode(const u32* values, 
                                       usize num_values,
                                       u32 bits,
                                       byte* output,
                                       usize output_size) noexcept {
        usize required = max_encoded_size(num_values, bits);
        if (output_size < required) return 0;
        if (bits == 0 || bits > 32) return 0;
        
        std::memset(output, 0, required);
        
        u64 bit_offset = 0;
        for (usize i = 0; i < num_values; ++i) {
            // Write value at bit_offset
            u64 byte_offset = bit_offset / 8;
            u32 bit_pos = static_cast<u32>(bit_offset % 8);
            
            u32 value = values[i];
            
            // Write across byte boundaries
            if (bit_pos + bits <= 32) {
                // Can write in one or two operations
                u32 available_in_current_byte = 8 - bit_pos;
                
                if (bits <= available_in_current_byte) {
                    // Fits in current byte
                    output[byte_offset] |= static_cast<byte>((value & ((1U << bits) - 1)) << bit_pos);
                } else {
                    // Spans multiple bytes
                    u32 low_bits = available_in_current_byte;
                    u32 high_bits = bits - low_bits;
                    
                    output[byte_offset] |= static_cast<byte>((value & ((1U << low_bits) - 1)) << bit_pos);
                    
                    // Write remaining bits
                    u32 remaining = value >> low_bits;
                    u32 bytes_needed = (high_bits + 7) / 8;
                    
                    for (u32 j = 0; j < bytes_needed && (byte_offset + 1 + j) < required; ++j) {
                        output[byte_offset + 1 + j] |= static_cast<byte>(remaining >> (j * 8));
                    }
                }
            }
            
            bit_offset += bits;
        }
        
        return required;
    }
    
    // Decode bit-packed values
    [[nodiscard]] static usize decode(const byte* input,
                                       usize input_size,
                                       u32 bits,
                                       u32* values,
                                       usize num_values) noexcept {
        usize required_bits = num_values * bits;
        usize required_bytes = (required_bits + 7) / 8;
        
        if (input_size < required_bytes) return 0;
        if (bits == 0 || bits > 32) return 0;
        
        u64 bit_offset = 0;
        u32 mask = bits == 32 ? 0xFFFFFFFF : ((1U << bits) - 1);
        
        for (usize i = 0; i < num_values; ++i) {
            u64 byte_offset = bit_offset / 8;
            u32 bit_pos = static_cast<u32>(bit_offset % 8);
            
            // Read across byte boundaries
            if (bit_pos + bits <= 32 && byte_offset + 4 <= input_size) {
                // Read 4 bytes and extract
                u32 window = 0;
                for (u32 j = 0; j < 4 && (byte_offset + j) < input_size; ++j) {
                    window |= static_cast<u32>(input[byte_offset + j]) << (j * 8);
                }
                
                values[i] = (window >> bit_pos) & mask;
            } else {
                values[i] = 0;
            }
            
            bit_offset += bits;
        }
        
        return required_bytes;
    }
};

// Delta-encoded bit packing
// Stores differences between consecutive values
class DeltaBitPackU32 {
public:
    [[nodiscard]] static usize max_encoded_size(usize num_values, u32 bits) noexcept {
        // Store first value as raw, then deltas
        return sizeof(u32) + BitPackU32::max_encoded_size(num_values > 0 ? num_values - 1 : 0, bits);
    }
    
    [[nodiscard]] static usize encode(const u32* values,
                                       usize num_values,
                                       u32 bits,
                                       byte* output,
                                       usize output_size) noexcept {
        if (num_values == 0) return 0;
        if (output_size < sizeof(u32)) return 0;
        
        // Write first value raw
        std::memcpy(output, values, sizeof(u32));
        
        if (num_values == 1) return sizeof(u32);
        
        // Calculate deltas
        // Note: caller must provide delta buffer or we use stack for small batches
        u32 delta_bits = bits;
        usize delta_size = BitPackU32::max_encoded_size(num_values - 1, delta_bits);
        
        if (output_size < sizeof(u32) + delta_size) return 0;
        
        // Encode deltas inline (simplified - in practice would use temp buffer)
        u32 prev = values[0];
        u64 bit_offset = 0;
        byte* delta_out = output + sizeof(u32);
        u32 delta_mask = delta_bits == 32 ? 0xFFFFFFFF : ((1U << delta_bits) - 1);
        
        for (usize i = 1; i < num_values; ++i) {
            u32 delta = values[i] - prev;
            prev = values[i];
            
            // Simple bit packing
            u64 byte_offset = bit_offset / 8;
            u32 bit_pos = static_cast<u32>(bit_offset % 8);
            
            if (byte_offset + 4 <= output_size - sizeof(u32)) {
                u32 window = 0;
                for (u32 j = 0; j < 4; ++j) {
                    window |= static_cast<u32>(delta_out[byte_offset + j]) << (j * 8);
                }
                window |= (delta & delta_mask) << bit_pos;
                for (u32 j = 0; j < 4; ++j) {
                    delta_out[byte_offset + j] = static_cast<byte>(window >> (j * 8));
                }
            }
            
            bit_offset += delta_bits;
        }
        
        return sizeof(u32) + delta_size;
    }
    
    [[nodiscard]] static usize decode(const byte* input,
                                       usize input_size,
                                       u32 bits,
                                       u32* values,
                                       usize num_values) noexcept {
        if (num_values == 0) return 0;
        if (input_size < sizeof(u32)) return 0;
        
        // Read first value raw
        std::memcpy(values, input, sizeof(u32));
        
        if (num_values == 1) return sizeof(u32);
        
        // Decode deltas
        usize delta_size = BitPackU32::max_encoded_size(num_values - 1, bits);
        if (input_size < sizeof(u32) + delta_size) return 0;
        
        const byte* delta_in = input + sizeof(u32);
        u64 bit_offset = 0;
        u32 delta_mask = bits == 32 ? 0xFFFFFFFF : ((1U << bits) - 1);
        
        for (usize i = 1; i < num_values; ++i) {
            u64 byte_offset = bit_offset / 8;
            u32 bit_pos = static_cast<u32>(bit_offset % 8);
            
            u32 delta = 0;
            if (byte_offset + 4 <= input_size - sizeof(u32)) {
                u32 window = 0;
                for (u32 j = 0; j < 4; ++j) {
                    window |= static_cast<u32>(delta_in[byte_offset + j]) << (j * 8);
                }
                delta = (window >> bit_pos) & delta_mask;
            }
            
            values[i] = values[i - 1] + delta;
            bit_offset += bits;
        }
        
        return sizeof(u32) + delta_size;
    }
};

// Zigzag encoding for signed integers
// Maps signed to unsigned: 0->0, -1->1, 1->2, -2->3, 2->4, ...
class ZigZag {
public:
    [[nodiscard]] static u32 encode(i32 value) noexcept {
        return static_cast<u32>((value << 1) ^ (value >> 31));
    }
    
    [[nodiscard]] static u64 encode(i64 value) noexcept {
        return static_cast<u64>((value << 1) ^ (value >> 63));
    }
    
    [[nodiscard]] static i32 decode(u32 value) noexcept {
        return static_cast<i32>((value >> 1) ^ -(static_cast<i32>(value) & 1));
    }
    
    [[nodiscard]] static i64 decode(u64 value) noexcept {
        return static_cast<i64>((value >> 1) ^ -(static_cast<i64>(value) & 1));
    }
};

} // namespace codec

} // namespace vectortick
