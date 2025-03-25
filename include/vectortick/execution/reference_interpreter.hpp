#pragma once

#include "../ir/function.hpp"
#include "../ir/instruction.hpp"
#include "../model/event.hpp"
#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include <unordered_map>
#include <vector>

namespace vectortick {

// Reference interpreter - executes IR row by row
// Used as the semantic oracle for testing
class ReferenceInterpreter {
public:
    ReferenceInterpreter() = default;
    
    // Execute a function with given row data
    // Returns the result value
    [[nodiscard]] Result<u64> execute(const ir::Function* function, 
                                       const CanonicalEvent* event) noexcept;
    
    // Execute a filter function
    [[nodiscard]] Result<bool> execute_filter(const ir::Function* function,
                                               const CanonicalEvent* event) noexcept;
    
    // Execute aggregation
    [[nodiscard]] Result<u64> execute_aggregate(const ir::Function* function,
                                                 const std::vector<CanonicalEvent>& events) noexcept;
    
    // Get execution statistics
    [[nodiscard]] u64 instructions_executed() const noexcept { return instructions_executed_; }
    void reset_stats() noexcept { instructions_executed_ = 0; }

private:
    // Value storage
    std::unordered_map<ir::ValueId, u64> values_;
    
    // Load value from column
    [[nodiscard]] u64 load_column_value(u32 column_id, const CanonicalEvent* event) const noexcept;
    
    // Execute a single instruction
    [[nodiscard]] Status execute_instruction(const ir::Instruction* instr,
                                              const CanonicalEvent* event) noexcept;
    
    // Execute a basic block
    [[nodiscard]] Status execute_block(ir::BasicBlock* block,
                                        const CanonicalEvent* event,
                                        ir::BasicBlock*& next_block) noexcept;
    
    // Checked arithmetic helpers
    [[nodiscard]] static std::optional<u64> checked_add_u64(u64 a, u64 b) noexcept;
    [[nodiscard]] static std::optional<u64> checked_sub_u64(u64 a, u64 b) noexcept;
    [[nodiscard]] static std::optional<u64> checked_mul_u64(u64 a, u64 b) noexcept;
    [[nodiscard]] static std::optional<i64> checked_add_i64(i64 a, i64 b) noexcept;
    [[nodiscard]] static std::optional<i64> checked_sub_i64(i64 a, i64 b) noexcept;
    [[nodiscard]] static std::optional<i64> checked_mul_i64(i64 a, i64 b) noexcept;
    
    u64 instructions_executed_ = 0;
};

} // namespace vectortick
