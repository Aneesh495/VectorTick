#include "vectortick/jit/code_generator.hpp"
#include <algorithm>
#include <cstring>

namespace vectortick {
namespace jit {

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
    
    // Allocate stack space for spills (64 bytes)
    assembler_.add_r64_imm32(X86Reg::RSP, -64);
}

void X86CodeGenerator::emit_epilogue() noexcept {
    // Restore stack pointer
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
        
        case Opcode::BitXor: {
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
            return Status(StatusCode::NotImplemented, "Instruction not implemented in x86 JIT");
    }
    
    return Status::OK();
}

X86Reg X86CodeGenerator::allocate_reg() noexcept {
    if (free_regs_.empty()) {
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

} // namespace jit
} // namespace vectortick
