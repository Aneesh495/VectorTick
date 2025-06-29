#include "vectortick/protocol/decoder.hpp"
#include "vectortick/protocol/frame.hpp"
#include "vectortick/model/event.hpp"

#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 65536) {
        return 0;
    }

    vectortick::vtp1::Decoder decoder;
    vectortick::CanonicalEvent event{};
    
    size_t offset = 0;
    while (offset < size) {
        auto result = decoder.decode_frame(
            reinterpret_cast<const vectortick::byte*>(data + offset),
            size - offset,
            event
        );
        if (!result.ok()) {
            break;
        }
        if (result.value() == 0) {
            break;
        }
        offset += result.value();
    }
    return 0;
}
