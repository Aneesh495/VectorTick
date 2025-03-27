#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include <vector>
#include <cstring>

namespace vectortick {

namespace jit {

// X86-64 register enumeration
enum class X86Reg : u8 {
    // General purpose 64-bit
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // 32-bit versions
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
    R8D = 8, R9D = 9, R10D = 10, R11D = 11,
    R12D = 12, R13D = 13, R14D = 14, R15D = 15,
    
    // XMM registers
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7,
    XMM8 = 8, XMM9 = 9, XMM10 = 10, XMM11 = 11,
    XMM12 = 12, XMM13 = 13, XMM14 = 14, XMM15 = 15,
};

// Condition codes for conditional instructions
enum class Condition : u8 {
    O  = 0x0,  // Overflow
    NO = 0x1,  // No overflow
    B  = 0x2,  // Below (unsigned <)
    NB = 0x3,  // Not below (unsigned >=)
    E  = 0x4,  // Equal
    NE = 0x5,  // Not equal
    BE = 0x6,  // Below or equal (unsigned <=)
    A  = 0x7,  // Above (unsigned >)
    S  = 0x8,  // Sign
    NS = 0x9,  // No sign
    L  = 0xC,  // Less (signed <)
    GE = 0xD,  // Greater or equal (signed >=)
    LE = 0xE,  // Less or equal (signed <=)
    G  = 0xF,  // Greater (signed >)
};

// X86-64 assembler - emits raw machine code
class X86Assembler {
public:
    X86Assembler() { code_.reserve(4096); }
    
    // Get the emitted code
    [[nodiscard]] const std::vector<u8>& code() const noexcept { return code_; }
    [[nodiscard]] u8* data() noexcept { return code_.data(); }
    [[nodiscard]] usize size() const noexcept { return code_.size(); }
    
    // Clear the code buffer
    void clear() noexcept { code_.clear(); }
    
    // Emit a byte
    void emit(u8 byte) { code_.push_back(byte); }
    
    // Emit multiple bytes
    void emit(const u8* bytes, usize count) {
        code_.insert(code_.end(), bytes, bytes + count);
    }
    
    // Emit 16-bit value (little-endian)
    void emit16(u16 value) {
        code_.push_back(static_cast<u8>(value));
        code_.push_back(static_cast<u8>(value >> 8));
    }
    
    // Emit 32-bit value (little-endian)
    void emit32(u32 value) {
        code_.push_back(static_cast<u8>(value));
        code_.push_back(static_cast<u8>(value >> 8));
        code_.push_back(static_cast<u8>(value >> 16));
        code_.push_back(static_cast<u8>(value >> 24));
    }
    
    // Emit 64-bit value (little-endian)
    void emit64(u64 value) {
        emit32(static_cast<u32>(value));
        emit32(static_cast<u32>(value >> 32));
    }
    
    // === REX prefixes ===
    void rex() { emit(0x40); }
    void rex_w() { emit(0x48); }  // 64-bit operand size
    void rex_r() { emit(0x44); }  // MODRM reg extension
    void rex_x() { emit(0x42); }  // SIB index extension
    void rex_b() { emit(0x41); }  // R/M extension
    void rex_wr() { emit(0x4C); }
    void rex_wb() { emit(0x49); }
    void rex_wrb() { emit(0x4D); }
    
    // === Move instructions ===
    
    // mov r64, imm64
    void mov_r64_imm64(X86Reg reg, u64 imm) {
        emit(0x48 | (static_cast<u8>(reg) >> 3));  // REX.W + reg bit
        emit(0xB8 | (static_cast<u8>(reg) & 7));   // MOV r64, imm64 opcode
        emit64(imm);
    }
    
    // mov r32, imm32
    void mov_r32_imm32(X86Reg reg, u32 imm) {
        emit(0xB8 | static_cast<u8>(reg));  // MOV r32, imm32 opcode
        emit32(imm);
    }
    
    // mov r64, r64
    void mov_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x89);  // MOV r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // mov r32, r32
    void mov_r32_r32(X86Reg dst, X86Reg src) {
        emit(0x89);  // MOV r/m32, r32
        emit(modrm(3, src, dst));
    }
    
    // mov r64, [mem]
    void mov_r64_mem(X86Reg dst, X86Reg base, i32 offset = 0) {
        emit(0x48);  // REX.W
        emit(0x8B);  // MOV r64, r/m64
        emit(modrm(0, dst, base));
        if (base == X86Reg::RBP || base == X86Reg::R13) {
            // Need displacement
            emit32(static_cast<u32>(offset));
        }
    }
    
    // mov [mem], r64
    void mov_mem_r64(X86Reg base, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x89);  // MOV r/m64, r64
        emit(modrm(0, src, base));
    }
    
    // === Arithmetic instructions ===
    
    // add r64, imm32 (sign-extended to 64)
    void add_r64_imm32(X86Reg dst, i32 imm) {
        emit(0x48);  // REX.W
        emit(0x81);  // ADD r/m64, imm32
        emit(modrm(3, 0, dst));
        emit32(static_cast<u32>(imm));
    }
    
    // add r64, r64
    void add_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x01);  // ADD r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // sub r64, r64
    void sub_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x29);  // SUB r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // imul r64, r64
    void imul_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x0F);
        emit(0xAF);  // IMUL r64, r/m64
        emit(modrm(3, dst, src));
    }
    
    // xor r64, r64
    void xor_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x31);  // XOR r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // and r64, r64
    void and_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x21);  // AND r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // or r64, r64
    void or_r64_r64(X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x09);  // OR r/m64, r64
        emit(modrm(3, src, dst));
    }
    
    // cmp r64, r64
    void cmp_r64_r64(X86Reg a, X86Reg b) {
        emit(0x48);  // REX.W
        emit(0x39);  // CMP r/m64, r64
        emit(modrm(3, b, a));
    }
    
    // test r64, r64
    void test_r64_r64(X86Reg a, X86Reg b) {
        emit(0x48);  // REX.W
        emit(0x85);  // TEST r/m64, r64
        emit(modrm(3, b, a));
    }
    
    // === Control flow ===
    
    // jmp rel32
    void jmp_rel32(i32 offset) {
        emit(0xE9);
        emit32(static_cast<u32>(offset));
    }
    
    // jmp r64
    void jmp_r64(X86Reg target) {
        emit(0xFF);
        emit(modrm(3, 4, target));
    }
    
    // call r64
    void call_r64(X86Reg target) {
        emit(0xFF);
        emit(modrm(3, 2, target));
    }
    
    // ret
    void ret() {
        emit(0xC3);
    }
    
    // nop
    void nop() {
        emit(0x90);
    }
    
    // push r64
    void push_r64(X86Reg reg) {
        if (static_cast<u8>(reg) >= 8) {
            emit(0x41);  // REX.B
        }
        emit(0x50 | (static_cast<u8>(reg) & 7));
    }
    
    // pop r64
    void pop_r64(X86Reg reg) {
        if (static_cast<u8>(reg) >= 8) {
            emit(0x41);  // REX.B
        }
        emit(0x58 | (static_cast<u8>(reg) & 7));
    }
    
    // === Conditional moves and sets ===
    
    // setcc r8
    void setcc(Condition cond, X86Reg dst) {
        emit(0x0F);
        emit(0x90 | static_cast<u8>(cond));
        emit(modrm(3, 0, dst));
    }
    
    // cmovcc r64, r64
    void cmovcc_r64_r64(Condition cond, X86Reg dst, X86Reg src) {
        emit(0x48);  // REX.W
        emit(0x0F);
        emit(0x40 | static_cast<u8>(cond));
        emit(modrm(3, src, dst));
    }
    
    // === SIMD instructions ===
    
    // pxor xmm, xmm
    void pxor_xmm_xmm(X86Reg dst, X86Reg src) {
        emit(0x66);
        if (static_cast<u8>(dst) >= 8 || static_cast<u8>(src) >= 8) {
            emit(0x40 | ((static_cast<u8>(dst) >> 3) << 2) | (static_cast<u8>(src) >> 3));
        }
        emit(0x0F);
        emit(0xEF);  // PXOR
        emit(modrm(3, src, dst));
    }
    
    // movdqu xmm, [mem]
    void movdqu_xmm_mem(X86Reg dst, X86Reg base) {
        emit(0xF3);
        if (static_cast<u8>(dst) >= 8) {
            emit(0x44);
        }
        emit(0x0F);
        emit(0x6F);  // MOVDQU
        emit(modrm(0, dst, base));
    }
    
    // movdqu [mem], xmm
    void movdqu_mem_xmm(X86Reg base, X86Reg src) {
        emit(0xF3);
        if (static_cast<u8>(src) >= 8) {
            emit(0x44);
        }
        emit(0x0F);
        emit(0x7F);  // MOVDQU
        emit(modrm(0, src, base));
    }
    
    // === Patching ===
    
    // Get current offset for patching
    [[nodiscard]] usize current_offset() const noexcept { return code_.size(); }
    
    // Patch a 32-bit value at given offset
    void patch32(usize offset, u32 value) {
        code_[offset] = static_cast<u8>(value);
        code_[offset + 1] = static_cast<u8>(value >> 8);
        code_[offset + 2] = static_cast<u8>(value >> 16);
        code_[offset + 3] = static_cast<u8>(value >> 24);
    }
    
private:
    std::vector<u8> code_;
    
    // Build MODRM byte
    [[nodiscard]] static u8 modrm(u8 mod, u8 reg, u8 rm) noexcept {
        return ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
    }
    
    // Build SIB byte
    [[nodiscard]] static u8 sib(u8 scale, u8 index, u8 base) noexcept {
        return ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
    }
};

} // namespace jit

} // namespace vectortick
