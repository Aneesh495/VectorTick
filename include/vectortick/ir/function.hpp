#pragma once

#include "basic_block.hpp"
#include "value.hpp"
#include "../common/types.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace vectortick {

namespace ir {

// Function in SSA IR
class Function {
public:
    explicit Function(const std::string& name = "")
        : name_(name), next_value_id_(1), entry_block_(nullptr), exit_block_(nullptr) {}
    
    // Function name
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    // Basic blocks
    [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] usize num_blocks() const noexcept { return blocks_.size(); }
    
    BasicBlock* create_block(const std::string& name = "") {
        auto block = std::make_unique<BasicBlock>(name);
        block->set_function(this);
        block->set_id(block_id_counter_++);
        
        if (blocks_.empty()) {
            entry_block_ = block.get();
        }
        
        blocks_.push_back(std::move(block));
        return blocks_.back().get();
    }
    
    [[nodiscard]] BasicBlock* block(usize idx) noexcept {
        return idx < blocks_.size() ? blocks_[idx].get() : nullptr;
    }
    
    [[nodiscard]] const BasicBlock* block(usize idx) const noexcept {
        return idx < blocks_.size() ? blocks_[idx].get() : nullptr;
    }
    
    // Entry and exit blocks
    [[nodiscard]] BasicBlock* entry_block() noexcept { return entry_block_; }
    [[nodiscard]] const BasicBlock* entry_block() const noexcept { return entry_block_; }
    
    [[nodiscard]] BasicBlock* exit_block() noexcept { return exit_block_; }
    [[nodiscard]] const BasicBlock* exit_block() const noexcept { return exit_block_; }
    
    void set_exit_block(BasicBlock* block) noexcept { exit_block_ = block; }
    
    // Value management
    ValueId create_value(Type type, const std::string& name = "") {
        ValueId id = next_value_id_++;
        values_[id] = std::make_unique<Value>(id, type, name);
        return id;
    }
    
    [[nodiscard]] Value* value(ValueId id) noexcept {
        auto it = values_.find(id);
        return it != values_.end() ? it->second.get() : nullptr;
    }
    
    [[nodiscard]] const Value* value(ValueId id) const noexcept {
        auto it = values_.find(id);
        return it != values_.end() ? it->second.get() : nullptr;
    }
    
    // Parameters
    void add_parameter(ValueId param) {
        parameters_.push_back(param);
    }
    
    [[nodiscard]] const std::vector<ValueId>& parameters() const noexcept { return parameters_; }
    [[nodiscard]] usize num_parameters() const noexcept { return parameters_.size(); }
    
    // Return type
    [[nodiscard]] Type return_type() const noexcept { return return_type_; }
    void set_return_type(Type type) noexcept { return_type_ = type; }
    
    // Statistics
    [[nodiscard]] usize num_values() const noexcept { return values_.size(); }
    [[nodiscard]] usize num_instructions() const noexcept {
        usize count = 0;
        for (const auto& block : blocks_) {
            count += block->num_instructions();
        }
        return count;
    }

private:
    std::string name_;
    std::vector<std::unique_ptr<BasicBlock>> blocks_;
    std::unordered_map<ValueId, std::unique_ptr<Value>> values_;
    std::vector<ValueId> parameters_;
    Type return_type_ = Type::Void;
    
    ValueId next_value_id_;
    usize block_id_counter_ = 0;
    
    BasicBlock* entry_block_;
    BasicBlock* exit_block_;
};

} // namespace ir

} // namespace vectortick
