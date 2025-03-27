#include "vectortick/jit/jit_compiler.hpp"
#include <cstring>
#include <sys/mman.h>

namespace vectortick {
namespace jit {

// === JitMemory implementation ===

JitMemory::JitMemory()
    : memory_(nullptr)
    , size_(0)
    , executable_(false) {}

JitMemory::~JitMemory() {
    if (memory_ && size_ > 0) {
        munmap(memory_, size_);
    }
}

Status JitMemory::allocate(usize size) noexcept {
    if (size == 0) {
        return Status(StatusCode::InvalidArgument, "Size must be > 0");
    }
    
    // Round up to page size
    usize page_size = 4096;
    usize alloc_size = ((size + page_size - 1) / page_size) * page_size;
    
    // Allocate memory with read/write/execute permissions
    void* mem = mmap(nullptr, alloc_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (mem == MAP_FAILED) {
        return Status(StatusCode::AllocationFailed, "mmap failed");
    }
    
    if (memory_ && size_ > 0) {
        munmap(memory_, size_);
    }
    
    memory_ = mem;
    size_ = alloc_size;
    executable_ = true;  // Already mapped with EXEC
    
    return Status::OK();
}

Status JitMemory::write(const u8* code, usize size) noexcept {
    if (!memory_ || size > size_) {
        return Status(StatusCode::InvalidArgument, "Invalid memory or size");
    }
    
    std::memcpy(memory_, code, size);
    return Status::OK();
}

Status JitMemory::make_executable() noexcept {
    if (!memory_ || size_ == 0) {
        return Status(StatusCode::InvalidArgument, "No memory allocated");
    }
    
    if (executable_) {
        return Status::OK();  // Already executable
    }
    
    return protect(PROT_READ | PROT_EXEC);
}

Status JitMemory::protect(int prot) noexcept {
    if (mprotect(memory_, size_, prot) != 0) {
        return Status(StatusCode::PermissionDenied, "mprotect failed");
    }
    executable_ = (prot & PROT_EXEC) != 0;
    return Status::OK();
}

// === JitCompiler implementation ===

Result<JitFunction> JitCompiler::compile(const ir::Function* function) noexcept {
    if (!function) {
        return make_error<JitFunction>(StatusCode::InvalidArgument, "Null function");
    }
    
    if (function->num_blocks() == 0) {
        return make_error<JitFunction>(StatusCode::InvalidArgument, "Function has no blocks");
    }
    
    // Reset state
    assembler_.clear();
    value_locations_.clear();
    free_regs_ = {
        X86Reg::RAX, X86Reg::RCX, X86Reg::RDX, X86Reg::RSI, X86Reg::RDI,
        X86Reg::R8, X86Reg::R9, X86Reg::R10, X86Reg::R11
    };
    
    // Emit prologue
    emit_prologue();
    
    // Compile each block
    for (usize i = 0; i < function->num_blocks(); ++i) {
        const ir::BasicBlock* block = function->block(i);
        auto status = compile_block(block);
        if (!status.ok()) {
            return make_error<JitFunction>(status.code(), status.message());
        }
    }
    
    // Emit epilogue (in case function doesn't return)
    emit_epilogue();
    
    // Allocate executable memory
    auto status = memory_.allocate(assembler_.size());
    if (!status.ok()) {
        return make_error<JitFunction>(status.code(), status.message());
    }
    
    // Copy code to executable memory
    status = memory_.write(assembler_.data(), assembler_.size());
    if (!status.ok()) {
        return make_error<JitFunction>(status.code(), status.message());
    }
    
    return memory_.function();
}

void JitCompiler::emit_prologue() noexcept {
    // Save callee-saved registers
    assembler_.push_r64(X86Reg::RBP);
    assembler_.mov_r64_r64(X86Reg::RBP, X86Reg::RSP);
    assembler_.push_r64(X86Reg::RBX);
    assembler_.push_r64(X86Reg::R12);
    assembler_.push_r64(X86Reg::R13);
    assembler_.push_r64(X86Reg::R14);
    assembler_.push_r64(X86Reg::R15);
    
    // Align stack to 16 bytes
    // Note: For simplicity, we assume stack is already aligned
}

void JitCompiler::emit_epilogue() noexcept {
    // Restore callee-saved registers
    assembler_.pop_r64(X86Reg::R15);
    assembler_.pop_r64(X86Reg::R14);
    assembler_.pop_r64(X86Reg::R13);
    assembler_.pop_r64(X86Reg::R12);
    assembler_.pop_r64(X86Reg::RBX);
    assembler_.pop_r64(X86Reg::RBP);
    assembler_.ret();
}

Status JitCompiler::compile_block(const ir::BasicBlock* block) noexcept {
    for (usize i = 0; i < block->num_instructions(); ++i) {
        const ir::Instruction* instr = block->instruction(i);
        auto status = compile_instruction(instr);
        if (!status.ok()) {
            return status;
        }
    }
    
    return Status::OK();
}

Status JitCompiler::compile_instruction(const ir::Instruction* instr) noexcept {
    using namespace ir;
    
    switch (instr->opcode()) {
        case Opcode::Nop:
            assembler_.nop();
            break;
            
        case Opcode::ConstU64:
        case Opcode::ConstI64: {
            auto const_op = static_cast<const ConstOp*>(instr);
            X86Reg dst = allocate_register();
            assembler_.mov_r64_imm64(dst, const_op->constant().get_u64());
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::ConstU32: {
            auto const_op = static_cast<const ConstOp*>(instr);
            X86Reg dst = allocate_register();
            assembler_.mov_r32_imm32(dst, static_cast<u32>(const_op->constant().get_u32()));
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::AddU64: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.add_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::SubU64: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.sub_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::MulU64: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.imul_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::And: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.and_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::Or: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.or_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::Xor: {
            X86Reg lhs = get_location(instr->operand(0));
            X86Reg rhs = get_location(instr->operand(1));
            X86Reg dst = allocate_register();
            assembler_.mov_r64_r64(dst, lhs);
            assembler_.xor_r64_r64(dst, rhs);
            set_location(instr->result(), dst);
            break;
        }
        
        case Opcode::Return: {
            if (instr->num_operands() > 0) {
                X86Reg ret_val = get_location(instr->operand(0));
                if (ret_val != X86Reg::RAX) {
                    assembler_.mov_r64_r64(X86Reg::RAX, ret_val);
                }
            } else {
                assembler_.xor_r64_r64(X86Reg::RAX, X86Reg::RAX);  // Return 0
            }
            emit_epilogue();
            break;
        }
        
        default:
            // Unsupported instruction - just skip
            break;
    }
    
    return Status::OK();
}

X86Reg JitCompiler::allocate_register() noexcept {
    if (free_regs_.empty()) {
        // Spill - for now just use RAX
        return X86Reg::RAX;
    }
    
    X86Reg reg = free_regs_.back();
    free_regs_.pop_back();
    return reg;
}

void JitCompiler::free_register(X86Reg reg) noexcept {
    free_regs_.push_back(reg);
}

X86Reg JitCompiler::get_location(ir::ValueId value) noexcept {
    auto it = value_locations_.find(value);
    if (it != value_locations_.end()) {
        return it->second;
    }
    // Return RAX as default
    return X86Reg::RAX;
}

void JitCompiler::set_location(ir::ValueId value, X86Reg reg) noexcept {
    value_locations_[value] = reg;
}

} // namespace jit
} // namespace vectortick
