#include "../test_framework.hpp"
#include "vectortick/common/types.hpp"
#include "vectortick/common/endian.hpp"
#include "vectortick/common/crc32c.hpp"
#include "vectortick/model/event.hpp"

using namespace vectortick;

VT_TEST(types_tests, integer_sizes) {
    VT_ASSERT_EQ(sizeof(u8), 1ULL);
    VT_ASSERT_EQ(sizeof(u16), 2ULL);
    VT_ASSERT_EQ(sizeof(u32), 4ULL);
    VT_ASSERT_EQ(sizeof(u64), 8ULL);
    VT_ASSERT_EQ(sizeof(i8), 1ULL);
    VT_ASSERT_EQ(sizeof(i16), 2ULL);
    VT_ASSERT_EQ(sizeof(i32), 4ULL);
    VT_ASSERT_EQ(sizeof(i64), 8ULL);
    VT_ASSERT_EQ(sizeof(f32), 4ULL);
    VT_ASSERT_EQ(sizeof(f64), 8ULL);
}

VT_TEST(types_tests, canonical_event_size) {
    VT_ASSERT_EQ(sizeof(CanonicalEvent), 56ULL);
}

VT_TEST(types_tests, endian_roundtrip) {
    u16 val16 = 0x1234;
    u32 val32 = 0x12345678;
    u64 val64 = 0x123456789ABCDEF0ULL;
    
    u16 net16 = host_to_network(val16);
    u32 net32 = host_to_network(val32);
    u64 net64 = host_to_network(val64);
    
    VT_ASSERT_EQ(network_to_host(net16), val16);
    VT_ASSERT_EQ(network_to_host(net32), val32);
    VT_ASSERT_EQ(network_to_host(net64), val64);
}

VT_TEST(types_tests, crc32c_known_vectors) {
    // Known-answer test vectors for CRC32C (Castagnoli 0x1EDC6F41):
    // "123456789" -> 0xE3069283
    const char* str = "123456789";
    u32 crc = crc32c(reinterpret_cast<const u8*>(str), 9);
    VT_ASSERT_EQ(crc, 0xE3069283U);

    // Empty buffer has CRC 0
    VT_ASSERT_EQ(crc32c(nullptr, 0), 0U);
}
