#include "../test_framework.hpp"
#include "vectortick/jit/x86_assembler.hpp"

using namespace vectortick;
using namespace vectortick::jit;

VT_TEST(x86_encoder_tests, basic_arithmetic_rax_rcx) {
    X86Assembler as;
    as.add_r64_r64(X86Reg::RAX, X86Reg::RCX);
    // REX.W (48) + 01 + ModRM(3, RCX=1, RAX=0) -> 48 01 c8
    const u8 expected[] = { 0x48, 0x01, 0xc8 };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, extended_registers_r8_r15) {
    X86Assembler as;
    // add r10, r11
    // REX.W + REX.R(r11) + REX.B(r10) = 0x4D
    // 01 + ModRM(3, reg=3, rm=2) -> c0 | (3<<3) | 2 = 0xda
    as.add_r64_r64(X86Reg::R10, X86Reg::R11);
    const u8 expected[] = { 0x4d, 0x01, 0xda };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, sub_imul_xor) {
    X86Assembler as;
    as.sub_r64_r64(X86Reg::RAX, X86Reg::RDX);
    // 48 29 d0
    as.imul_r64_r64(X86Reg::RAX, X86Reg::RBX);
    // 48 0f af c3
    as.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
    // 48 31 c0
    const u8 expected[] = {
        0x48, 0x29, 0xd0,
        0x48, 0x0f, 0xaf, 0xc3,
        0x48, 0x31, 0xc0
    };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, mov_imm64) {
    X86Assembler as;
    as.mov_r64_imm64(X86Reg::RAX, 0x0102030405060708ULL);
    // 48 b8 08 07 06 05 04 03 02 01
    as.mov_r64_imm64(X86Reg::R8, 0x0102030405060708ULL);
    // 49 b8 08 07 06 05 04 03 02 01
    const u8 expected[] = {
        0x48, 0xb8, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x49, 0xb8, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
    };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, push_pop_extended) {
    X86Assembler as;
    as.push_r64(X86Reg::RAX); // 50
    as.push_r64(X86Reg::R12); // 41 54
    as.pop_r64(X86Reg::R12);  // 41 5c
    as.pop_r64(X86Reg::RAX);  // 58
    as.ret();                 // c3
    const u8 expected[] = { 0x50, 0x41, 0x54, 0x41, 0x5c, 0x58, 0xc3 };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, memory_displacement_and_sib) {
    X86Assembler as;
    // mov rax, [rcx]
    as.mov_r64_mem(X86Reg::RAX, X86Reg::RCX, 0);
    // mov rax, [rcx + 16]
    as.mov_r64_mem(X86Reg::RAX, X86Reg::RCX, 16);
    // mov rax, [rsp] -> needs SIB
    as.mov_r64_mem(X86Reg::RAX, X86Reg::RSP, 0);
    // mov rax, [rsp + 8] -> needs SIB + disp8
    as.mov_r64_mem(X86Reg::RAX, X86Reg::RSP, 8);
    // mov rax, [rbp] -> needs disp8 = 0
    as.mov_r64_mem(X86Reg::RAX, X86Reg::RBP, 0);

    const u8 expected[] = {
        0x48, 0x8b, 0x01,
        0x48, 0x8b, 0x41, 0x10,
        0x48, 0x8b, 0x04, 0x24,
        0x48, 0x8b, 0x44, 0x24, 0x08,
        0x48, 0x8b, 0x45, 0x00
    };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}

VT_TEST(x86_encoder_tests, condition_codes_setcc) {
    X86Assembler as;
    as.setcc(Condition::E, X86Reg::RAX);  // 0f 94 c0
    as.setcc(Condition::NE, X86Reg::RAX); // 0f 95 c0
    as.setcc(Condition::AE, X86Reg::RAX); // 0f 93 c0
    as.setcc(Condition::B, X86Reg::RAX);  // 0f 92 c0
    as.setcc(Condition::GE, X86Reg::RAX); // 0f 9d c0
    as.setcc(Condition::L, X86Reg::RAX);  // 0f 9c c0

    const u8 expected[] = {
        0x0f, 0x94, 0xc0,
        0x0f, 0x95, 0xc0,
        0x0f, 0x93, 0xc0,
        0x0f, 0x92, 0xc0,
        0x0f, 0x9d, 0xc0,
        0x0f, 0x9c, 0xc0
    };
    VT_ASSERT_EQ(as.size(), sizeof(expected));
    VT_ASSERT_BYTES_EQ(as.data(), expected, sizeof(expected));
}
