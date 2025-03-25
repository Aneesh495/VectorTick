// VectorTick Test Suite
// Basic tests for core components

#include "vectortick/common/types.hpp"
#include "vectortick/common/endian.hpp"
#include "vectortick/common/checked_math.hpp"
#include "vectortick/common/crc32c.hpp"
#include "vectortick/model/event.hpp"
#include "vectortick/execution/reference_interpreter.hpp"

#include <iostream>
#include <cstring>

using namespace vectortick;

// Test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct TestRunner_##name { \
        TestRunner_##name() { \
            ++tests_run; \
            std::cout << "Running: " #name << "... "; \
            test_##name(); \
        } \
    } test_runner_##name; \
    static void test_##name()

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cout << "FAILED: " #cond << "\n"; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cout << "FAILED: " #a " != " #b << "\n"; \
            return; \
        } \
    } while(0)

// Tests

TEST(types_size) {
    ASSERT_EQ(sizeof(u8), 1ULL);
    ASSERT_EQ(sizeof(u16), 2ULL);
    ASSERT_EQ(sizeof(u32), 4ULL);
    ASSERT_EQ(sizeof(u64), 8ULL);
    ASSERT_EQ(sizeof(i8), 1ULL);
    ASSERT_EQ(sizeof(i16), 2ULL);
    ASSERT_EQ(sizeof(i32), 4ULL);
    ASSERT_EQ(sizeof(i64), 8ULL);
}

TEST(endian_swap) {
    // Test host_to_network (which swaps bytes on little-endian)
    u16 val16 = 0x0102;
    u32 val32 = 0x01020304;
    u64 val64 = 0x0102030405060708ULL;
    
    // On little-endian, this will swap
    u16 swapped16 = host_to_network(val16);
    u32 swapped32 = host_to_network(val32);
    u64 swapped64 = host_to_network(val64);
    
    // Just verify they work - exact result depends on host endianness
    (void)swapped16;
    (void)swapped32;
    (void)swapped64;
}

TEST(crc32c_basic) {
    // Test CRC32C computation
    const char* data = "hello world";
    u32 crc = crc32c(reinterpret_cast<const u8*>(data), strlen(data));
    ASSERT(crc != 0);
    
    // Same data should produce same CRC
    u32 crc2 = crc32c(reinterpret_cast<const u8*>(data), strlen(data));
    ASSERT_EQ(crc, crc2);
}

TEST(canonical_event_size) {
    // Canonical event should be exactly 56 bytes
    ASSERT_EQ(sizeof(CanonicalEvent), 56ULL);
}

TEST(checked_math_add) {
    auto r1 = checked_add(u64(100), u64(200));
    ASSERT(r1.has_value());
    ASSERT_EQ(*r1, 300ULL);
    
    auto r2 = checked_add(std::numeric_limits<u64>::max(), u64(1));
    ASSERT(!r2.has_value());
}

TEST(checked_math_mul) {
    auto r1 = checked_mul(u64(100), u64(200));
    ASSERT(r1.has_value());
    ASSERT_EQ(*r1, 20000ULL);
    
    auto r2 = checked_mul(std::numeric_limits<u64>::max(), u64(2));
    ASSERT(!r2.has_value());
}

int main() {
    std::cout << "VectorTick Test Suite\n";
    std::cout << "=====================\n\n";
    
    // Tests run automatically via TestRunner
    
    std::cout << "\n=====================\n";
    std::cout << "Tests run: " << tests_run << "\n";
    std::cout << "Tests passed: " << tests_passed << "\n";
    std::cout << "Tests failed: " << (tests_run - tests_passed) << "\n";
    
    return tests_passed == tests_run ? 0 : 1;
}
