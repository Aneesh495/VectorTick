#include "vectortick/jit/code_generator.hpp"
#include <algorithm>
#include <cstring>

namespace vectortick {
namespace jit {

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
    assembler_.stp_pre(A64Reg::X29, A64Reg::X30, A64Reg::SP, -16);
    assembler_.mov_x64_x64(A64Reg::X29, A64Reg::SP);
    
    // Save callee-saved registers
    assembler_.stp_pre(A64Reg::X19, A64Reg::X20, A64Reg::SP, -16);
    assembler_.stp_pre(A64Reg::X21, A64Reg::X22, A64Reg::SP, -16);
    assembler_.stp_pre(A64Reg::X23, A64Reg::X24, A64Reg::SP, -16);
    assembler_.stp_pre(A64Reg::X25, A64Reg::X26, A64Reg::SP, -16);
    assembler_.stp_pre(A64Reg::X27, A64Reg::X28, A64Reg::SP, -16);
}

void A64CodeGenerator::emit_epilogue() noexcept {
    // Restore callee-saved registers
    assembler_.ldp_post(A64Reg::X27, A64Reg::X28, A64Reg::SP, 16);
    assembler_.ldp_post(A64Reg::X25, A64Reg::X26, A64Reg::SP, 16);
    assembler_.ldp_post(A64Reg::X23, A64Reg::X24, A64Reg::SP, 16);
    assembler_.ldp_post(A64Reg::X21, A64Reg::X22, A64Reg::SP, 16);
    assembler_.ldp_post(A64Reg::X19, A64Reg::X20, A64Reg::SP, 16);
    
    // Restore frame pointer and return
    assembler_.ldp_post(A64Reg::X29, A64Reg::X30, A64Reg::SP, 16);
    assembler_.ret();
}

Status A64CodeGenerator::emit_instruction(const ir::Instruction* instr) noexcept {
    using namespace ir;
    
    switch (instr->opcode()) {
        case Opcode::Nop:
            assembler_.nop();
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
            assembler_.movz_x64(dst, static_cast<u16>(const_op->constant().get_u32() & 0xFFFF), 0);
            if (const_op->constant().get_u32() > 0xFFFF) {
                assembler_.movk_x64(dst, static_cast<u16>(const_op->constant().get_u32() >> 16), 16);
            }
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
        
        case Opcode::BitXor: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.eor_x64_x64_x64(dst, lhs, rhs);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::EqU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::EQ);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::NeU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::NE);
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::LtU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::LO); // Unsigned < (CC/LO)
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::LeU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::LS); // Unsigned <=
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::GtU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::HI); // Unsigned >
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::GeU64: {
            A64Reg lhs = get_reg(instr->operand(0));
            A64Reg rhs = get_reg(instr->operand(1));
            A64Reg dst = allocate_reg();
            assembler_.cmp_x64_x64(lhs, rhs);
            assembler_.cset_x64(dst, A64Condition::HS); // Unsigned >= (CS/HS)
            set_reg(instr->result(), dst);
            break;
        }
        
        case Opcode::Return: {
            if (instr->num_operands() > 0) {
                A64Reg ret_val = get_reg(instr->operand(0));
                if (ret_val != A64Reg::X0) {
                    assembler_.mov_x64_x64(A64Reg::X0, ret_val);
                }
            } else {
                assembler_.mov_x64_imm64(A64Reg::X0, 0);
            }
            emit_epilogue();
            break;
        }
        
        default:
            return Status(StatusCode::NotImplemented, "Instruction not implemented in AArch64 JIT");
    }
    
    return Status::OK();
}

A64Reg A64CodeGenerator::allocate_reg() noexcept {
    if (free_regs_.empty()) {
        return A64Reg::X0;
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
