#include "../test_framework.hpp"
#include "vectortick/common/checked_math.hpp"
#include <limits>

using namespace vectortick;

VT_TEST(checked_arithmetic_tests, unsigned_addition) {
    auto r1 = checked_add(u64(100), u64(200));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, 300ULL);
    
    auto r2 = checked_add(std::numeric_limits<u64>::max(), u64(1));
    VT_ASSERT(!r2.has_value());
}

VT_TEST(checked_arithmetic_tests, signed_addition) {
    auto r1 = checked_add(i64(100), i64(-200));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, -100LL);
    
    auto r2 = checked_add(std::numeric_limits<i64>::max(), i64(1));
    VT_ASSERT(!r2.has_value());
    
    auto r3 = checked_add(std::numeric_limits<i64>::min(), i64(-1));
    VT_ASSERT(!r3.has_value());
}

VT_TEST(checked_arithmetic_tests, unsigned_subtraction) {
    auto r1 = checked_sub(u64(200), u64(100));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, 100ULL);
    
    auto r2 = checked_sub(u64(100), u64(200));
    VT_ASSERT(!r2.has_value());
}

VT_TEST(checked_arithmetic_tests, signed_subtraction) {
    auto r1 = checked_sub(i64(100), i64(200));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, -100LL);
    
    auto r2 = checked_sub(std::numeric_limits<i64>::min(), i64(1));
    VT_ASSERT(!r2.has_value());
    
    auto r3 = checked_sub(std::numeric_limits<i64>::max(), i64(-1));
    VT_ASSERT(!r3.has_value());
}

VT_TEST(checked_arithmetic_tests, unsigned_multiplication) {
    auto r1 = checked_mul(u64(100), u64(200));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, 20000ULL);
    
    auto r2 = checked_mul(std::numeric_limits<u64>::max(), u64(2));
    VT_ASSERT(!r2.has_value());
}

VT_TEST(checked_arithmetic_tests, signed_multiplication) {
    auto r1 = checked_mul(i64(-100), i64(200));
    VT_ASSERT(r1.has_value());
    VT_ASSERT_EQ(*r1, -20000LL);
    
    auto r2 = checked_mul(std::numeric_limits<i64>::max(), i64(2));
    VT_ASSERT(!r2.has_value());
    
    auto r3 = checked_mul(std::numeric_limits<i64>::min(), i64(-1));
    VT_ASSERT(!r3.has_value());
}
