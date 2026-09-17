#include "vectortick/jit/code_generator.hpp"
#include <algorithm>
#include <cstring>

namespace vectortick {
namespace jit {

// === CodeGenerator base implementation ===

void CodeGenerator::analyze_function(const ir::Function* function) noexcept {
    count_uses(function);
    build_interference(function);
}

void CodeGenerator::count_uses(const ir::Function* function) noexcept {
    reg_info_.clear();
    
    for (usize b = 0; b < function->num_blocks(); ++b) {
        const ir::BasicBlock* block = function->block(b);
        
        for (usize i = 0; i < block->num_instructions(); ++i) {
            const ir::Instruction* instr = block->instruction(i);
            
            // Count uses of operands
            for (usize o = 0; o < instr->num_operands(); ++o) {
                u32 value_id = instr->operand(o);
                reg_info_[value_id].use_count++;
            }
            
            // Count definitions
            if (instr->result() != 0) {
                reg_info_[instr->result()].def_count++;
            }
        }
    }
}

void CodeGenerator::build_interference(const ir::Function* function) noexcept {
    // Simple interference building - two values interfere if both are live at any point
    // For now, just use a linear scan approach
    (void)function;  // TODO: implement proper interference graph
}

// === X86CodeGenerator implementation ===

Result<std::vector<u8>> X86CodeGenerator::generate(const ir::Function* function) noexcept {
    if (!function) {
        return make_error<std::vector<u8>>(StatusCode::InvalidArgument, "Null function");
    }
    
    // Reset state
    assembler_.clear();
    value_to_reg_.clear();
    free_regs_ = {
        X86Reg::RAX, X86Reg::RCX, X86Reg::RDX, X86Reg::RSI, X86Reg::RDI,
        X86Reg::R8, X86Reg::R9, X86Reg::R10, X86Reg::R11
    };
    
    // Analyze function
    analyze_function(function);
    
    // Emit prologue
    emit_prologue();
    
    // Generate code for each block
    for (usize b = 0; b < function->num_blocks(); ++b) {
        const ir::BasicBlock* block = function->block(b);
        
        for (usize i = 0; i < block->num_instructions(); ++i) {
            const ir::Instruction* instr = block->instruction(i);
            auto status = emit_instruction(instr);
            if (!status.ok()) {
                return make_error<std::vector<u8>>(status.code(), status.message());
            }
        }
    }
    
    // Emit epilogue (in case function doesn't return)
    emit_epilogue();
    
    // Copy code to vector
    std::vector<u8> code(assembler_.code());
    return code;
}

void X86CodeGenerator::emit_prologue() noexcept {
    // Save callee-saved registers
    assembler_.push_r64(X86Reg::RBP);
    assembler_.mov_r64_r64(X86Reg::RBP, X86Reg::RSP);
    assembler_.push_r64(X86Reg::RBX);
    assembler_.push_r64(X86Reg::R12);
    assembler_.push_r64(X86Reg::R13);
    assembler_.push_r64(X86Reg::R14);
    assembler_.push_r64(X86Reg::R15);
    
    // Allocate stack space for spills (simplified - 64 bytes)
    assembler_.add_r64_imm32(X86Reg::RSP, -64);
}

void X86CodeGenerator::emit_epilogue() noexcept {
    // Restore stack
    assembler_.add_r64_imm32(X86Reg::RSP, 64);
    
    // Restore callee-saved registers
    assembler_.pop_r64(X86Reg::R15);
    assembler_.pop_r64(X86Reg::R14);
    assembler_.pop_r64(X86Reg::R13);
    assembler_.pop_r64(X86Reg::R12);
    assembler_.pop_r64(X86Reg::RBX);
    assembler_.pop_r64(X86Reg::RBP);
    assembler_.ret();
}

Status X86CodeGenerator::emit_instruction(const ir::Instruction* instr) noexcept {
    using namespace ir;
    
    switch (instr->opcode()) {
        case Opcode::Nop:
            assembler_.nop();
            break;
            
        case Opcode::ConstU64:
        case Opcode::ConstI64: {
            auto const_op = static_cast<const ConstOp*>(instr);
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_imm64(dst, const_op->constant().get_u64());
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::ConstU32: {
            auto const_op = static_cast<const ConstOp*>(instr);
            X86Reg dst = allocate_reg();
            assembler_.mov_r32_imm32(dst, const_op->constant().get_u32());
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::ConstBool: {
            auto const_op = static_cast<const ConstOp*>(instr);
            X86Reg dst = allocate_reg();
            assembler_.mov_r32_imm32(dst, const_op->constant().get_bool() ? 1 : 0);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::AddU64:
        case Opcode::AddI64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.add_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::SubU64:
        case Opcode::SubI64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.sub_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::MulU64:
        case Opcode::MulI64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.imul_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::And: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.and_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Or: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.or_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Xor: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            X86Reg dst = allocate_reg();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.xor_r64_r64(dst, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::EqU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);  // Zero result
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::E, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::NeU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::NE, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::LtU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::B, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::LeU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::BE, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::GtU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::A, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::GeU64: {
            X86Reg lhs = get_reg(instr->operand(0));
            X86Reg rhs = get_reg(instr->operand(1));
            assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            assembler_.cmp_r64_r64(lhs, rhs);
            assembler_.setcc(Condition::AE, X86Reg::RAX);
            set_reg(instr->result(), X86Reg::RAX);
            break;
        }
        
        case Opcode::Return: {
            if (instr->num_operands() > 0) {
                X86Reg ret_val = get_reg(instr->operand(0));
                if (ret_val != X86Reg::RAX) {
                    assembler_.mov_r64_r64(X86Reg::RAX, ret_val);
                }
            } else {
                assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);
            }
            emit_epilogue();
            break;
        }
        
        default:
            // Unsupported - emit NOP
            assembler_.nop();
            break;
    }
    
    return Status::OK();
}

X86Reg X86CodeGenerator::allocate_reg() noexcept {
    if (free_regs_.empty()) {
        // Spill - use RAX
        return X86Reg::RAX;
    }
    X86Reg reg = free_regs_.back();
    free_regs_.pop_back();
    return reg;
}

void X86CodeGenerator::free_reg(X86Reg reg) noexcept {
    free_regs_.push_back(reg);
}

X86Reg X86CodeGenerator::get_reg(u32 value_id) noexcept {
    auto it = value_to_reg_.find(value_id);
    return it != value_to_reg_.end() ? it->second : X86Reg::RAX;
}

void X86CodeGenerator::set_reg(u32 value_id, X86Reg reg) noexcept {
    value_to_reg_[value_id] = reg;
}

// === A64CodeGenerator implementation ===

Result<std::vector<u8>> A64CodeGenerator::generate(const ir::Function* function) noexcept {
    if (!function) {
        return make_error<std::vector<u8>>(StatusCode::InvalidArgument, "Null function");
    }
    
    // Reset state
    assembler_.clear();
    value_to_reg_.clear();
    free_regs_ = {
        A64Reg::X0, A64Reg::X1, A64Reg::X2, A64Reg::X3, A64Reg::X4,
        A64Reg::X5, A64Reg::X6, A64Reg::X7, A64Reg::X8, A64Reg::X9,
        A64Reg::X10, A64Reg::X11, A64Reg::X12, A64Reg::X13, A64Reg::X14,
        A64Reg::X15, A64Reg::X16, A64Reg::X17, A64Reg::X18
    };
    
    // Analyze function
    analyze_function(function);
    
    // Emit prologue
    emit_prologue();
    
    // Generate code for each block
    for (usize b = 0; b < function->num_blocks(); ++b) {
        const ir::BasicBlock* block = function->block(b);
        
        for (usize i = 0; i < block->num_instructions(); ++i) {
            const ir::Instruction* instr = block->instruction(i);
            auto status = emit_instruction(instr);
            if (!status.ok()) {
                return make_error<std::vector<u8>>(status.code(), status.message());
            }
        }
    }
    
    // Emit epilogue
    emit_epilogue();
    
    // Copy code to vector of u8
    const u32* code32 = assembler_.data();
    usize num_bytes = assembler_.size_bytes();
    std::vector<u8> code(num_bytes);
    std::memcpy(code.data(), code32, num_bytes);
    
    return code;
}

void A64CodeGenerator::emit_prologue() noexcept {
    // Save frame pointer and link register
    // stp x29, x30, [sp, #-16]!
    assembler_.stp_pre(A64Reg::X29, A64Reg::X30, -16);
    assembler_.mov_x64_x64(A64Reg::X29, A64Reg::SP);
    
    // Save callee-saved registers
    // stp x19, x20, [sp, #-16]!
    assembler_.stp_pre(A64Reg::X19, A64Reg::X20, -16);
    assembler_.stp_pre(A64Reg::X21, A64Reg::X22, -16);
    assembler_.stp_pre(A64Reg::X23, A64Reg::X24, -16);
    assembler_.stp_pre(A64Reg::X25, A64Reg::X26, -16);
    assembler_.stp_pre(A64Reg::X27, A64Reg::X28, -16);
}

void A64CodeGenerator::emit_epilogue() noexcept {
    // Restore callee-saved registers
    assembler_.ldp_pre(A64Reg::X27, A64Reg::X28, 16);
    assembler_.ldp_pre(A64Reg::X25, A64Reg::X26, 16);
    assembler_.ldp_pre(A64Reg::X23, A64Reg::X24, 16);
    assembler_.ldp_pre(A64Reg::X21, A64Reg::X22, 16);
    assembler_.ldp_pre(A64Reg::X19, A64Reg::X20, 16);
    
    // Restore frame pointer and return
    assembler_.ldp_pre(A64Reg::X29, A64Reg::X30, 16);
    assembler_.ret();
}

Status A64CodeGenerator::emit_instruction(const ir::Instruction* instr) noexcept {
    using namespace ir;
    
    switch (instr->opcode()) {
        case Opcode::Nop:
            // NOP is encoded as HINT #0
            assembler_.emit(0xD503201F);
            break;
            
        case Opcode::ConstU64:
        case Opcode::ConstI64: {
            auto const_op = static_cast<const ConstOp*>(instr);
            A64Reg dst = allocate_reg();
            assembler_.mov_x64_imm64(dst, const_op->constant().get_u64());
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::ConstU32: {
            auto const_op = static_cast<const ConstOp*>(instr);
            A64Reg dst = allocate_reg();
            assembler_.movz_x64(dst, const_op->constant().get_u32(), 0);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::AddU64:
        case Opcode::AddI64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.add_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::SubU64:
        case Opcode::SubI64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.sub_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::MulU64:
        case Opcode::MulI64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.mul_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::And: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.and_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Or: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.orr_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Xor: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.eor_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Return: {
            if (instr->num_operands() > 0) {
                A64Reg ret_val = get_reg(instr->operand(0));
                if (ret_val != A64Reg::X0) {
                    assembler_.mov_x64_x64(A64Reg::X0, ret_val);
                }
            }
            emit_epilogue();
            break;
        }
        
        default:
            // Unsupported - emit NOP
            assembler_.emit(0xD503201F);
            break;
    }
    
    return Status::OK();
}

A64Reg A64CodeGenerator::allocate_reg() noexcept {
    if (free_regs_.empty()) {
        return A64Reg::X0;  // Spill
    }
    A64Reg reg = free_regs_.back();
    free_regs_.pop_back();
    return reg;
}

void A64CodeGenerator::free_reg(A64Reg reg) noexcept {
    free_regs_.push_back(reg);
}

A64Reg A64CodeGenerator::get_reg(u32 value_id) noexcept {
    auto it = value_to_reg_.find(value_id);
    return it != value_to_reg_.end() ? it->second : A64Reg::X0;
}

void A64CodeGenerator::set_reg(u32 value_id, A64Reg reg) noexcept {
    value_to_reg_[value_id] = reg;
}

} // namespace jit
} // namespace vectortick
