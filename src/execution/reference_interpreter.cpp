#include "vectortick/execution/reference_interpreter.hpp"
#include "vectortick/common/result.hpp"
#include "vectortick/storage/file_format.hpp"
#include <limits>

namespace vectortick {

Result<u64> ReferenceInterpreter::execute(const ir::Function* function,
                                           const CanonicalEvent* event) noexcept {
    if (!function || !event) {
        return make_error<u64>(StatusCode::InvalidArgument, "Null function or event");
    }
    
    // Clear value storage
    values_.clear();
    
    // Start at entry block
    const ir::BasicBlock* current = function->entry_block();
    
    while (current) {
        // Execute each instruction in block
        for (usize i = 0; i < current->num_instructions(); ++i) {
            const ir::Instruction* instr = current->instruction(i);
            
            auto status = execute_instruction(instr, event);
            if (!status.ok()) {
                return make_error<u64>(status.code(), status.message());
            }
            
            ++instructions_executed_;
            
            // Check for terminator
            if (instr->is_terminator()) {
                if (instr->opcode() == ir::Opcode::Return) {
                    // Return the value if present
                    if (instr->num_operands() > 0) {
                        return values_[instr->operand(0)];
                    }
                    return 0ULL;
                }
                break;
            }
        }
    }
    
    return make_error<u64>(StatusCode::InternalError, "Function did not return");
}

Result<bool> ReferenceInterpreter::execute_filter(const ir::Function* function,
                                                   const CanonicalEvent* event) noexcept {
    auto result = execute(function, event);
    if (!result.ok()) {
        return make_error<bool>(result.status().code(), result.status().message());
    }
    return result.value() != 0;
}

Result<u64> ReferenceInterpreter::execute_aggregate(const ir::Function* function,
                                                     const std::vector<CanonicalEvent>& events) noexcept {
    // For aggregates, we execute over all events
    u64 result = 0;
    
    for (const auto& event : events) {
        auto r = execute(function, &event);
        if (!r.ok()) {
            return r;
        }
        result = r.value();
    }
    
    return result;
}

u64 ReferenceInterpreter::load_column_value(u32 column_id, const CanonicalEvent* event) const noexcept {
    // Column IDs match the storage format
    switch (column_id) {
        case 0:  // ExchangeTsNs
            return event->exchange_ts_ns;
        case 1:  // ReceiveTsNs
            return event->receive_ts_ns;
        case 2:  // Sequence
            return event->sequence;
        case 3:  // InstrumentId
            return event->instrument_id;
        case 4:  // EventType
            return static_cast<u64>(event->event_type);
        case 5:  // Side
            return static_cast<u64>(event->side);
        case 6:  // Flags
            return event->flags;
        case 7:  // PriceTicks
            return static_cast<u64>(event->price_ticks);
        case 8:  // Quantity
            return event->quantity;
        case 9:  // VenueId
            return event->venue_id;
        case 10: // SourceId
            return event->source_id;
        case 11: // TradeOrOrderId
            return event->trade_or_order_id;
        default:
            return 0;
    }
}

Status ReferenceInterpreter::execute_instruction(const ir::Instruction* instr,
                                                   const CanonicalEvent* event) noexcept {
    ir::Opcode op = instr->opcode();
    ir::ValueId result = instr->result();
    
    switch (op) {
        case ir::Opcode::Nop:
            break;
            
        case ir::Opcode::ConstI64:
        case ir::Opcode::ConstU64:
        case ir::Opcode::ConstU32:
        case ir::Opcode::ConstBool: {
            // Constants are stored in instruction
            auto const_op = static_cast<const ir::ConstOp*>(instr);
            values_[result] = const_op->constant().get_u64();
            break;
        }
        
        case ir::Opcode::LoadColumn: {
            auto load = static_cast<const ir::LoadColumnOp*>(instr);
            u64 value = load_column_value(load->column_id(), event);
            values_[result] = value;
            break;
        }
        
        case ir::Opcode::AddU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            auto sum = checked_add_u64(lhs, rhs);
            if (!sum) {
                return Status(StatusCode::InternalError, "Addition overflow");
            }
            values_[result] = *sum;
            break;
        }
        
        case ir::Opcode::SubU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            auto diff = checked_sub_u64(lhs, rhs);
            if (!diff) {
                return Status(StatusCode::InternalError, "Subtraction underflow");
            }
            values_[result] = *diff;
            break;
        }
        
        case ir::Opcode::MulU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            auto prod = checked_mul_u64(lhs, rhs);
            if (!prod) {
                return Status(StatusCode::InternalError, "Multiplication overflow");
            }
            values_[result] = *prod;
            break;
        }
        
        case ir::Opcode::DivU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            if (rhs == 0) {
                return Status(StatusCode::InternalError, "Division by zero");
            }
            values_[result] = lhs / rhs;
            break;
        }
        
        case ir::Opcode::ModU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            if (rhs == 0) {
                return Status(StatusCode::InternalError, "Modulo by zero");
            }
            values_[result] = lhs % rhs;
            break;
        }
        
        case ir::Opcode::AddI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            auto sum = checked_add_i64(lhs, rhs);
            if (!sum) {
                return Status(StatusCode::InternalError, "Addition overflow");
            }
            values_[result] = static_cast<u64>(*sum);
            break;
        }
        
        case ir::Opcode::SubI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            auto diff = checked_sub_i64(lhs, rhs);
            if (!diff) {
                return Status(StatusCode::InternalError, "Subtraction underflow");
            }
            values_[result] = static_cast<u64>(*diff);
            break;
        }
        
        case ir::Opcode::MulI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            auto prod = checked_mul_i64(lhs, rhs);
            if (!prod) {
                return Status(StatusCode::InternalError, "Multiplication overflow");
            }
            values_[result] = static_cast<u64>(*prod);
            break;
        }
        
        case ir::Opcode::DivI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            if (rhs == 0) {
                return Status(StatusCode::InternalError, "Division by zero");
            }
            values_[result] = static_cast<u64>(lhs / rhs);
            break;
        }
        
        case ir::Opcode::NegI64: {
            i64 val = static_cast<i64>(values_[instr->operand(0)]);
            if (val == std::numeric_limits<i64>::min()) {
                return Status(StatusCode::InternalError, "Negation overflow");
            }
            values_[result] = static_cast<u64>(-val);
            break;
        }
        
        case ir::Opcode::EqU64:
        case ir::Opcode::EqI64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs == rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::NeU64:
        case ir::Opcode::NeI64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs != rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::LtU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs < rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::LeU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs <= rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::GtU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs > rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::GeU64: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs >= rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::LtI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            values_[result] = (lhs < rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::LeI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            values_[result] = (lhs <= rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::GtI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            values_[result] = (lhs > rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::GeI64: {
            i64 lhs = static_cast<i64>(values_[instr->operand(0)]);
            i64 rhs = static_cast<i64>(values_[instr->operand(1)]);
            values_[result] = (lhs >= rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::And: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs && rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::Or: {
            u64 lhs = values_[instr->operand(0)];
            u64 rhs = values_[instr->operand(1)];
            values_[result] = (lhs || rhs) ? 1 : 0;
            break;
        }
        
        case ir::Opcode::Not: {
            u64 val = values_[instr->operand(0)];
            values_[result] = val ? 0 : 1;
            break;
        }
        
        case ir::Opcode::BitAnd: {
            values_[result] = values_[instr->operand(0)] & values_[instr->operand(1)];
            break;
        }
        
        case ir::Opcode::BitOr: {
            values_[result] = values_[instr->operand(0)] | values_[instr->operand(1)];
            break;
        }
        
        case ir::Opcode::BitXor: {
            values_[result] = values_[instr->operand(0)] ^ values_[instr->operand(1)];
            break;
        }
        
        case ir::Opcode::BitNot: {
            values_[result] = ~values_[instr->operand(0)];
            break;
        }
        
        case ir::Opcode::Select: {
            u64 cond = values_[instr->operand(0)];
            u64 true_val = values_[instr->operand(1)];
            u64 false_val = values_[instr->operand(2)];
            values_[result] = cond ? true_val : false_val;
            break;
        }
        
        default:
            return Status(StatusCode::NotImplemented, "Opcode not implemented");
    }
    
    return Status::OK();
}

std::optional<u64> ReferenceInterpreter::checked_add_u64(u64 a, u64 b) noexcept {
    if (a > std::numeric_limits<u64>::max() - b) {
        return std::nullopt;
    }
    return a + b;
}

std::optional<u64> ReferenceInterpreter::checked_sub_u64(u64 a, u64 b) noexcept {
    if (a < b) {
        return std::nullopt;
    }
    return a - b;
}

std::optional<u64> ReferenceInterpreter::checked_mul_u64(u64 a, u64 b) noexcept {
    if (a > 0 && b > std::numeric_limits<u64>::max() / a) {
        return std::nullopt;
    }
    return a * b;
}

std::optional<i64> ReferenceInterpreter::checked_add_i64(i64 a, i64 b) noexcept {
    if (b > 0 && a > std::numeric_limits<i64>::max() - b) {
        return std::nullopt;
    }
    if (b < 0 && a < std::numeric_limits<i64>::min() - b) {
        return std::nullopt;
    }
    return a + b;
}

std::optional<i64> ReferenceInterpreter::checked_sub_i64(i64 a, i64 b) noexcept {
    if (b > 0 && a < std::numeric_limits<i64>::min() + b) {
        return std::nullopt;
    }
    if (b < 0 && a > std::numeric_limits<i64>::max() + b) {
        return std::nullopt;
    }
    return a - b;
}

std::optional<i64> ReferenceInterpreter::checked_mul_i64(i64 a, i64 b) noexcept {
    if (a == 0 || b == 0) return 0;
    if (a == 1) return b;
    if (b == 1) return a;
    
    if (a > 0) {
        if (b > 0) {
            if (a > std::numeric_limits<i64>::max() / b) return std::nullopt;
        } else {
            if (b < std::numeric_limits<i64>::min() / a) return std::nullopt;
        }
    } else {
        if (b > 0) {
            if (a < std::numeric_limits<i64>::min() / b) return std::nullopt;
        } else {
            if (a < std::numeric_limits<i64>::max() / b) return std::nullopt;
        }
    }
    
    return a * b;
}

} // namespace vectortick
