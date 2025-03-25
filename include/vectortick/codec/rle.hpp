#pragma once

#include "../common/types.hpp"
#include "../common/endian.hpp"
#include <cstring>

namespace vectortick {

namespace codec {

// Run-Length Encoding for low-cardinality data
// Format: [count: u16][value: T] repeated
// Count of 0 means literal run follows: [count:0][n:u16][n values]

class RLE {
public:
    // Maximum run length
    static constexpr u16 MaxRunLength = 32767;
    static constexpr u16 LiteralMarker = 0;
    
    // Calculate max encoded size
    template <typename T>
    [[nodiscard]] static usize max_encoded_size(usize num_values) noexcept {
        // Worst case: all literals
        return num_values * (sizeof(u16) + sizeof(T));
    }
    
    // Encode values using RLE
    template <typename T>
    [[nodiscard]] static usize encode(const T* values, 
                                       usize num_values,
                                       byte* output,
                                       usize output_size) noexcept {
        usize offset = 0;
        usize i = 0;
        
        while (i < num_values) {
            // Count run length
            T run_value = values[i];
            usize run_length = 1;
            
            while (i + run_length < num_values && 
                   values[i + run_length] == run_value &&
                   run_length < MaxRunLength) {
                ++run_length;
            }
            
            if (run_length >= 4) {
                // Encode as RLE run
                if (offset + sizeof(u16) + sizeof(T) > output_size) return 0;
                
                write_be_u16(output + offset, static_cast<u16>(run_length));
                offset += sizeof(u16);
                
                std::memcpy(output + offset, &run_value, sizeof(T));
                offset += sizeof(T);
                
                i += run_length;
            } else {
                // Encode as literals
                // Find end of literal run
                usize literal_count = 0;
                
                while (i + literal_count < num_values && literal_count < MaxRunLength) {
                    // Check if next position starts a run
                    if (i + literal_count + 3 < num_values &&
                        values[i + literal_count] == values[i + literal_count + 1] &&
                        values[i + literal_count] == values[i + literal_count + 2] &&
                        values[i + literal_count] == values[i + literal_count + 3]) {
                        break;  // Start of a run
                    }
                    ++literal_count;
                }
                
                if (literal_count == 0) literal_count = 1;  // At least one
                
                // Write literal marker and count
                if (offset + sizeof(u16) * 2 + literal_count * sizeof(T) > output_size) return 0;
                
                write_be_u16(output + offset, LiteralMarker);
                offset += sizeof(u16);
                
                write_be_u16(output + offset, static_cast<u16>(literal_count));
                offset += sizeof(u16);
                
                // Write values
                for (usize j = 0; j < literal_count; ++j) {
                    std::memcpy(output + offset, &values[i + j], sizeof(T));
                    offset += sizeof(T);
                }
                
                i += literal_count;
            }
        }
        
        return offset;
    }
    
    // Decode RLE-encoded values
    template <typename T>
    [[nodiscard]] static usize decode(const byte* input,
                                       usize input_size,
                                       T* values,
                                       usize num_values) noexcept {
        usize in_offset = 0;
        usize out_count = 0;
        
        while (out_count < num_values && in_offset < input_size) {
            if (in_offset + sizeof(u16) > input_size) return 0;
            
            u16 count = read_be_u16(input + in_offset);
            in_offset += sizeof(u16);
            
            if (count == LiteralMarker) {
                // Literal run
                if (in_offset + sizeof(u16) > input_size) return 0;
                u16 literal_count = read_be_u16(input + in_offset);
                in_offset += sizeof(u16);
                
                if (out_count + literal_count > num_values) return 0;
                if (in_offset + literal_count * sizeof(T) > input_size) return 0;
                
                for (usize j = 0; j < literal_count; ++j) {
                    std::memcpy(&values[out_count++], input + in_offset, sizeof(T));
                    in_offset += sizeof(T);
                }
            } else {
                // RLE run
                if (in_offset + sizeof(T) > input_size) return 0;
                if (out_count + count > num_values) return 0;
                
                T value;
                std::memcpy(&value, input + in_offset, sizeof(T));
                in_offset += sizeof(T);
                
                for (u16 j = 0; j < count; ++j) {
                    values[out_count++] = value;
                }
            }
        }
        
        return out_count == num_values ? in_offset : 0;
    }
};

} // namespace codec

} // namespace vectortick
