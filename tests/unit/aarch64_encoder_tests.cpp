#include "../test_framework.hpp"
#include "vectortick/jit/a64_assembler.hpp"

using namespace vectortick;
using namespace vectortick::jit;

VT_TEST(aarch64_encoder_tests, ret_and_nop) {
    A64Assembler as;
    as.ret();
    as.nop();
    
    // RET X30 = 0xD65F03C0 -> C0 03 5F D6
    // NOP = 0xD503201F     -> 1F 20 03 D5
    const u32 expected[] = { 0xD65F03C0, 0xD503201F };
    VT_ASSERT_EQ(as.instruction_count(), 2ULL);
    VT_ASSERT_EQ(as.code()[0], expected[0]);
    VT_ASSERT_EQ(as.code()[1], expected[1]);
}

VT_TEST(aarch64_encoder_tests, mov_registers) {
    A64Assembler as;
    // MOV X0, X1 is ORR X0, XZR, X1
    as.mov_x64_x64(A64Reg::X0, A64Reg::X1);
    // MOV X19, X20
    as.mov_x64_x64(A64Reg::X19, A64Reg::X20);
    
    // ORR X0, XZR, X1 = 0xAA0103E0
    // ORR X19, XZR, X20 = 0xAA1403F3
    const u32 expected[] = { 0xAA0103E0, 0xAA1403F3 };
    VT_ASSERT_EQ(as.instruction_count(), 2ULL);
    VT_ASSERT_EQ(as.code()[0], expected[0]);
    VT_ASSERT_EQ(as.code()[1], expected[1]);
}

VT_TEST(aarch64_encoder_tests, basic_arithmetic) {
    A64Assembler as;
    // ADD X0, X1, X2 = 0x8B020020
    as.add_x64_x64_x64(A64Reg::X0, A64Reg::X1, A64Reg::X2);
    // SUB X3, X4, X5 = 0xCB050083
    as.sub_x64_x64_x64(A64Reg::X3, A64Reg::X4, A64Reg::X5);
    // MUL X6, X7, X8 = 0x9B087CE6 (MADD X6, X7, X8, XZR)
    as.mul_x64_x64_x64(A64Reg::X6, A64Reg::X7, A64Reg::X8);
    // AND X9, X10, X11 = 0x8A0B0149
    as.and_x64_x64_x64(A64Reg::X9, A64Reg::X10, A64Reg::X11);
    // ORR X12, X13, X14 = 0xAA0E01AC
    as.orr_x64_x64_x64(A64Reg::X12, A64Reg::X13, A64Reg::X14);
    // EOR X15, X16, X17 = 0xCA11020F
    as.eor_x64_x64_x64(A64Reg::X15, A64Reg::X16, A64Reg::X17);
    
    const u32 expected[] = {
        0x8B020020,
        0xCB050083,
        0x9B087CE6,
        0x8A0B0149,
        0xAA0E01AC,
        0xCA11020F
    };
    VT_ASSERT_EQ(as.instruction_count(), 6ULL);
    for (usize i = 0; i < 6; ++i) {
        VT_ASSERT_EQ(as.code()[i], expected[i]);
    }
}

VT_TEST(aarch64_encoder_tests, comparisons_and_cset) {
    A64Assembler as;
    // CMP X0, X1 = SUBS XZR, X0, X1 = 0xEB01001F
    as.cmp_x64_x64(A64Reg::X0, A64Reg::X1);
    // CSET X0, EQ = CSINC X0, XZR, XZR, NE(1) = 0x9A9F17E0
    as.cset_x64(A64Reg::X0, A64Condition::EQ);
    // CSET X0, NE = CSINC X0, XZR, XZR, EQ(0) = 0x9A9F07E0
    as.cset_x64(A64Reg::X0, A64Condition::NE);
    
    const u32 expected[] = {
        0xEB01001F,
        0x9A9F17E0,
        0x9A9F07E0
    };
    VT_ASSERT_EQ(as.instruction_count(), 3ULL);
    for (usize i = 0; i < 3; ++i) {
        VT_ASSERT_EQ(as.code()[i], expected[i]);
    }
}

VT_TEST(aarch64_encoder_tests, memory_and_stack) {
    A64Assembler as;
    // LDR X0, [X1, #16] -> pimm = 2 -> 0xF9400820
    as.ldr_x64(A64Reg::X0, A64Reg::X1, 16);
    // STR X2, [X3, #24] -> pimm = 3 -> 0xF9000C62
    as.str_x64(A64Reg::X2, A64Reg::X3, 24);
    // STP X29, X30, [SP, #-16]! -> 0xA9BF7BFD
    as.stp_pre(A64Reg::X29, A64Reg::X30, A64Reg::SP, -16);
    // LDP X29, X30, [SP], #16   -> 0xA8C17BFD
    as.ldp_post(A64Reg::X29, A64Reg::X30, A64Reg::SP, 16);
    
    const u32 expected[] = {
        0xF9400820,
        0xF9000C62,
        0xA9BF7BFD,
        0xA8C17BFD
    };
    VT_ASSERT_EQ(as.instruction_count(), 4ULL);
    for (usize i = 0; i < 4; ++i) {
        VT_ASSERT_EQ(as.code()[i], expected[i]);
    }
}
