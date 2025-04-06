#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include <vector>
#include <cstring>

namespace vectortick {

namespace jit {

// AArch64 register enumeration
enum class A64Reg : u8 {
    // General purpose registers (64-bit)
    X0 = 0, X1 = 1, X2 = 2, X3 = 3,
    X4 = 4, X5 = 5, X6 = 6, X7 = 7,
    X8 = 8, X9 = 9, X10 = 10, X11 = 11,
    X12 = 12, X13 = 13, X14 = 14, X15 = 15,
    X16 = 16, X17 = 17, X18 = 18, X19 = 19,
    X20 = 20, X21 = 21, X22 = 22, X23 = 23,
    X24 = 24, X25 = 25, X26 = 26, X27 = 27,
    X28 = 28, X29 = 29, X30 = 30, SP = 31,
    
    // 32-bit versions (W registers)
    W0 = 0, W1 = 1, W2 = 2, W3 = 3,
    W4 = 4, W5 = 5, W6 = 6, W7 = 7,
    W8 = 8, W9 = 9, W10 = 10, W11 = 11,
    W12 = 12, W13 = 13, W14 = 14, W15 = 15,
    W16 = 16, W17 = 17, W18 = 18, W19 = 19,
    W20 = 20, W21 = 21, W22 = 22, W23 = 23,
    W24 = 24, W25 = 25, W26 = 26, W27 = 27,
    W28 = 28, W29 = 29, W30 = 30, WZR = 31,
    
    // Vector registers (128-bit)
    V0 = 0, V1 = 1, V2 = 2, V3 = 3,
    V4 = 4, V5 = 5, V6 = 6, V7 = 7,
    V8 = 8, V9 = 9, V10 = 10, V11 = 11,
    V12 = 12, V13 = 13, V14 = 14, V15 = 15,
    V16 = 16, V17 = 17, V18 = 18, V19 = 19,
    V20 = 20, V21 = 21, V22 = 22, V23 = 23,
    V24 = 24, V25 = 25, V26 = 26, V27 = 27,
    V28 = 28, V29 = 29, V30 = 30, V31 = 31,
};

// AArch64 condition codes
enum class A64Condition : u8 {
    EQ = 0x0,  // Equal
    NE = 0x1,  // Not equal
    CS = 0x2,  // Carry set (unsigned >=)
    HS = 0x2,  // Unsigned >= (alias for CS)
    CC = 0x3,  // Carry clear (unsigned <)
    LO = 0x3,  // Unsigned < (alias for CC)
    MI = 0x4,  // Minus/negative
    PL = 0x5,  // Plus/positive or zero
    VS = 0x6,  // Overflow
    VC = 0x7,  // No overflow
    HI = 0x8,  // Unsigned >
    LS = 0x9,  // Unsigned <=
    GE = 0xA,  // Signed >=
    LT = 0xB,  // Signed <
    GT = 0xC,  // Signed >
    LE = 0xD,  // Signed <=
    AL = 0xE,  // Always
};

// AArch64 shift types
enum class A64Shift : u8 {
    LSL = 0x0,  // Logical shift left
    LSR = 0x1,  // Logical shift right
    ASR = 0x2,  // Arithmetic shift right
    ROR = 0x3,  // Rotate right
};

// AArch64 extend types
enum class A64Extend : u8 {
    UXTB = 0x0,  // Zero-extend byte
    UXTH = 0x1,  // Zero-extend halfword
    UXTW = 0x2,  // Zero-extend word
    UXTX = 0x3,  // Zero-extend doubleword (identity)
    SXTB = 0x4,  // Sign-extend byte
    SXTH = 0x5,  // Sign-extend halfword
    SXTW = 0x6,  // Sign-extend word
    SXTX = 0x7,  // Sign-extend doubleword (identity)
};

// AArch64 assembler - emits raw machine code
class A64Assembler {
public:
    A64Assembler() { code_.reserve(4096); }
    
    // Get the emitted code
    [[nodiscard]] const std::vector<u32>& code() const noexcept { return code_; }
    [[nodiscard]] u32* data() noexcept { return reinterpret_cast<u32*>(code_.data()); }
    [[nodiscard]] usize size() const noexcept { return code_.size() * 4; }
    [[nodiscard]] usize size_bytes() const noexcept { return code_.size() * sizeof(u32); }
    
    // Clear the code buffer
    void clear() noexcept { code_.clear(); }
    
    // Emit a 32-bit instruction (AArch64 instructions are always 32-bit)
    void emit(u32 instr) { code_.push_back(instr); }
    
    // === Move instructions ===
    
    // mov xD, xN
    void mov_x64_x64(A64Reg dst, A64Reg src) {
        // ORR Xd, XZR, Xm
        emit((0b10101010000 << 21) | (static_cast<u32>(src) << 16) | static_cast<u32>(dst));
    }
    
    // mov wD, wN
    void mov_w32_w32(A64Reg dst, A64Reg src) {
        // ORR Wd, WZR, Wm
        emit((0b00101010000 << 21) | (static_cast<u32>(src) << 16) | static_cast<u32>(dst));
    }
    
    // movz xD, #imm16, lsl #shift
    void movz_x64(A64Reg dst, u16 imm, u8 shift = 0) {
        u32 hw = shift / 16;
        emit((0b110100101 << 23) | (hw << 21) | (static_cast<u32>(imm) << 5) | static_cast<u32>(dst));
    }
    
    // movk xD, #imm16, lsl #shift (keep other bits)
    void movk_x64(A64Reg dst, u16 imm, u8 shift = 0) {
        u32 hw = shift / 16;
        emit((0b111100101 << 23) | (hw << 21) | (static_cast<u32>(imm) << 5) | static_cast<u32>(dst));
    }
    
    // mov xD, #imm64 (multiple instruction sequence)
    void mov_x64_imm64(A64Reg dst, u64 imm) {
        // Use movz/movk sequence
        movz_x64(dst, static_cast<u16>(imm & 0xFFFF), 0);
        if (imm & 0xFFFF0000ULL) {
            movk_x64(dst, static_cast<u16>((imm >> 16) & 0xFFFF), 16);
        }
        if (imm & 0xFFFF00000000ULL) {
            movk_x64(dst, static_cast<u16>((imm >> 32) & 0xFFFF), 32);
        }
        if (imm & 0xFFFF000000000000ULL) {
            movk_x64(dst, static_cast<u16>((imm >> 48) & 0xFFFF), 48);
        }
    }
    
    // === Arithmetic instructions ===
    
    // add xD, xN, xM
    void add_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10001011000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // add wD, wN, wM
    void add_w32_w32_w32(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b00001011000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // add xD, xN, #imm12
    void add_x64_imm12(A64Reg dst, A64Reg src, u16 imm) {
        emit((0b100100010 << 23) | (static_cast<u32>(imm) << 10) | 
             (static_cast<u32>(src) << 5) | static_cast<u32>(dst));
    }
    
    // sub xD, xN, xM
    void sub_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b11001011000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // mul xD, xN, xM
    void mul_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10011011000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // udiv xD, xN, xM
    void udiv_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10011010110 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // sdiv xD, xN, xM
    void sdiv_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10011010110 << 21) | (1 << 10) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // and xD, xN, xM
    void and_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10001010000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // orr xD, xN, xM
    void orr_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b10101010000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // eor xD, xN, xM (XOR)
    void eor_x64_x64_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
        emit((0b11001010000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | static_cast<u32>(dst));
    }
    
    // === Comparison instructions ===
    
    // cmp xN, xM (alias for subs xzr, xN, xM)
    void cmp_x64_x64(A64Reg src1, A64Reg src2) {
        emit((0b11101011000 << 21) | (static_cast<u32>(src2) << 16) | 
             static_cast<u32>(src1) << 5 | 31);  // xzr
    }
    
    // ccmp xN, xM, #nzcv, cond
    void ccmp_x64_x64(A64Reg src1, A64Reg src2, u8 nzcv, A64Condition cond) {
        emit((0b111010010 << 23) | (static_cast<u32>(src2) << 16) | 
             (static_cast<u32>(cond) << 12) | (static_cast<u32>(src1) << 5) | 
             ((static_cast<u32>(nzcv) & 0xF) << 0));
    }
    
    // === Branch instructions ===
    
    // b label (unconditional branch)
    void b(i32 offset) {
        // offset is in instructions, not bytes
        u32 imm26 = static_cast<u32>(offset) & 0x3FFFFFF;
        emit((0b000101 << 26) | imm26);
    }
    
    // bl label (branch with link)
    void bl(i32 offset) {
        u32 imm26 = static_cast<u32>(offset) & 0x3FFFFFF;
        emit((0b100101 << 26) | imm26);
    }
    
    // br xN (branch to register)
    void br(A64Reg target) {
        emit((0b1101011000011111000000 << 10) | static_cast<u32>(target));
    }
    
    // blr xN (branch with link to register)
    void blr(A64Reg target) {
        emit((0b1101011000111111000000 << 10) | static_cast<u32>(target));
    }
    
    // ret
    void ret() {
        emit(0xD65F03C0);
    }
    
    // b.cond label
    void b_cond(A64Condition cond, i32 offset) {
        // offset is in instructions
        u32 imm19 = (static_cast<u32>(offset) & 0x7FFFF) << 5;
        emit((0b01010100 << 24) | imm19 | static_cast<u32>(cond));
    }
    
    // === Load/Store instructions ===
    
    // ldr xD, [xN, #offset]
    void ldr_x64(A64Reg dst, A64Reg base, i32 offset) {
        u32 imm12 = static_cast<u32>(offset / 8) << 10;
        emit((0b1111100101 << 22) | imm12 | (static_cast<u32>(base) << 5) | 
             static_cast<u32>(dst));
    }
    
    // str xD, [xN, #offset]
    void str_x64(A64Reg src, A64Reg base, i32 offset) {
        u32 imm12 = static_cast<u32>(offset / 8) << 10;
        emit((0b1111100100 << 22) | imm12 | (static_cast<u32>(base) << 5) | 
             static_cast<u32>(src));
    }
    
    // ldur xD, [xN, #offset] (unscaled offset)
    void ldur_x64(A64Reg dst, A64Reg base, i32 offset) {
        u32 imm9 = (static_cast<u32>(offset) & 0x1FF) << 12;
        emit((0b11111000010 << 21) | imm9 | (1 << 10) | 
             (static_cast<u32>(base) << 5) | static_cast<u32>(dst));
    }
    
    // stur xD, [xN, #offset] (unscaled offset)
    void stur_x64(A64Reg src, A64Reg base, i32 offset) {
        u32 imm9 = (static_cast<u32>(offset) & 0x1FF) << 12;
        emit((0b11111000000 << 21) | imm9 | (1 << 10) | 
             (static_cast<u32>(base) << 5) | static_cast<u32>(src));
    }
    
    // === Stack operations ===
    
    // stp xN, xM, [sp, #offset]! (pre-index)
    void stp_pre(A64Reg src1, A64Reg src2, i32 offset) {
        u32 imm7 = (static_cast<u32>(offset / 8) & 0x7F) << 15;
        emit((0b1010100110 << 22) | imm7 | (static_cast<u32>(src2) << 10) | 
             (static_cast<u32>(src1) << 5));
    }
    
    // ldp xN, xM, [sp, #offset]! (pre-index)
    void ldp_pre(A64Reg dst1, A64Reg dst2, i32 offset) {
        u32 imm7 = (static_cast<u32>(offset / 8) & 0x7F) << 15;
        emit((0b1010100111 << 22) | imm7 | (static_cast<u32>(dst2) << 10) | 
             (static_cast<u32>(dst1) << 5));
    }
    
    // === Conditional select ===
    
    // csel xD, xN, xM, cond
    void csel_x64(A64Reg dst, A64Reg src1, A64Reg src2, A64Condition cond) {
        emit((0b10011010100 << 21) | (static_cast<u32>(src2) << 16) | 
             (static_cast<u32>(cond) << 12) | (static_cast<u32>(src1) << 5) | 
             static_cast<u32>(dst));
    }
    
    // cset xD, cond (set to 1 if condition true, else 0)
    void cset_x64(A64Reg dst, A64Condition cond) {
        // CSINC Xd, XZR, XZR, invert(cond)
        u32 inv_cond = static_cast<u32>(cond) ^ 1;
        emit((0b10011010100 << 21) | (31 << 16) | (inv_cond << 12) | 
             (31 << 5) | static_cast<u32>(dst));
    }
    
    // === Patching ===
    
    [[nodiscard]] usize current_offset() const noexcept { return code_.size(); }
    
    void patch_branch(usize offset, i32 target) {
        u32 instr = code_[offset];
        u32 imm26 = static_cast<u32>(target) & 0x3FFFFFF;
        code_[offset] = (instr & 0xFC000000) | imm26;
    }

private:
    std::vector<u32> code_;  // AArch64 instructions are 32-bit
};

} // namespace jit

} // namespace vectortick
