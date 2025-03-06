#pragma once

#include "instruction.hpp"
#include "../common/types.hpp"
#include <vector>
#include <memory>
#include <string>

namespace vectortick {

namespace ir {

// Forward declarations
class Function;

// Basic block - sequence of instructions with single entry and single exit
class BasicBlock {
public:
    explicit BasicBlock(const std::string& name = "")
        : name_(name), function_(nullptr) {}
    
    // Block name
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    // Parent function
    [[nodiscard]] Function* function() noexcept { return function_; }
    [[nodiscard]] const Function* function() const noexcept { return function_; }
    void set_function(Function* func) noexcept { function_ = func; }
    
    // Instructions
    [[nodiscard]] const std::vector<std::unique_ptr<Instruction>>& instructions() const noexcept { return instructions_; }
    [[nodiscard]] usize num_instructions() const noexcept { return instructions_.size(); }
    
    void append(std::unique_ptr<Instruction> instr) {
        instr->set_parent(this);
        instr->set_id(instruction_id_counter_++);
        instructions_.push_back(std::move(instr));
    }
    
    [[nodiscard]] Instruction* instruction(usize idx) noexcept {
        return idx < instructions_.size() ? instructions_[idx].get() : nullptr;
    }
    
    [[nodiscard]] const Instruction* instruction(usize idx) const noexcept {
        return idx < instructions_.size() ? instructions_[idx].get() : nullptr;
    }
    
    // Terminators
    [[nodiscard]] bool has_terminator() const noexcept {
        return !instructions_.empty() && instructions_.back()->is_terminator();
    }
    
    [[nodiscard]] Instruction* terminator() noexcept {
        return has_terminator() ? instructions_.back().get() : nullptr;
    }
    
    [[nodiscard]] const Instruction* terminator() const noexcept {
        return has_terminator() ? instructions_.back().get() : nullptr;
    }
    
    // Predecessors and successors
    [[nodiscard]] const std::vector<BasicBlock*>& predecessors() const noexcept { return predecessors_; }
    [[nodiscard]] const std::vector<BasicBlock*>& successors() const noexcept { return successors_; }
    
    void add_predecessor(BasicBlock* block) {
        predecessors_.push_back(block);
    }
    
    void add_successor(BasicBlock* block) {
        successors_.push_back(block);
    }
    
    // Block ID
    [[nodiscard]] usize id() const noexcept { return id_; }
    void set_id(usize id) noexcept { id_ = id; }
    
    // Check if block is empty
    [[nodiscard]] bool is_empty() const noexcept { return instructions_.empty(); }
    
    // Get first instruction
    [[nodiscard]] Instruction* first() noexcept {
        return instructions_.empty() ? nullptr : instructions_.front().get();
    }
    
    [[nodiscard]] const Instruction* first() const noexcept {
        return instructions_.empty() ? nullptr : instructions_.front().get();
    }
    
    // Get last instruction
    [[nodiscard]] Instruction* last() noexcept {
        return instructions_.empty() ? nullptr : instructions_.back().get();
    }
    
    [[nodiscard]] const Instruction* last() const noexcept {
        return instructions_.empty() ? nullptr : instructions_.back().get();
    }

private:
    std::string name_;
    Function* function_;
    std::vector<std::unique_ptr<Instruction>> instructions_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;
    usize id_ = 0;
    usize instruction_id_counter_ = 0;
};

} // namespace ir

} // namespace vectortick
