#include "vectortick/ir/builder.hpp"
#include <cassert>

namespace vectortick {
namespace ir {

Function* Builder::create_function(const std::string& name) {
    function_ = new Function(name);
    next_temp_ = 1;
    current_block_ = function_->create_block("entry");
    return function_;
}

BasicBlock* Builder::create_block(const std::string& name) {
    assert(function_ && "No function created");
    return function_->create_block(name);
}

ValueId Builder::create_const_i64(i64 value) {
    ValueId result = function_->create_value(Type::I64, "const");
    auto instr = std::make_unique<ConstOp>(Constant::i64(value), result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_const_u64(u64 value) {
    ValueId result = function_->create_value(Type::U64, "const");
    auto instr = std::make_unique<ConstOp>(Constant::u64(value), result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_const_u32(u32 value) {
    ValueId result = function_->create_value(Type::U32, "const");
    auto instr = std::make_unique<ConstOp>(Constant::u32(value), result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_const_bool(bool value) {
    ValueId result = function_->create_value(Type::I1, "const");
    auto instr = std::make_unique<ConstOp>(Constant::boolean(value), result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_add(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(type, "add");
    Opcode op = (type == Type::I64) ? Opcode::AddI64 : Opcode::AddU64;
    auto instr = std::make_unique<BinaryOp>(op, lhs, rhs, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_sub(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(type, "sub");
    Opcode op = (type == Type::I64) ? Opcode::SubI64 : Opcode::SubU64;
    auto instr = std::make_unique<BinaryOp>(op, lhs, rhs, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_mul(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(type, "mul");
    Opcode op = (type == Type::I64) ? Opcode::MulI64 : Opcode::MulU64;
    auto instr = std::make_unique<BinaryOp>(op, lhs, rhs, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_div(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(type, "div");
    Opcode op = (type == Type::I64) ? Opcode::DivI64 : Opcode::DivU64;
    auto instr = std::make_unique<BinaryOp>(op, lhs, rhs, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_mod(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(type, "mod");
    Opcode op = (type == Type::I64) ? Opcode::ModI64 : Opcode::ModU64;
    auto instr = std::make_unique<BinaryOp>(op, lhs, rhs, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_neg(ValueId operand, Type type) {
    ValueId result = function_->create_value(type, "neg");
    auto instr = std::make_unique<UnaryOp>(Opcode::NegI64, operand, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_eq(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "eq");
    Opcode op = (type == Type::I64) ? Opcode::EqI64 : Opcode::EqU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_ne(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "ne");
    Opcode op = (type == Type::I64) ? Opcode::NeI64 : Opcode::NeU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_lt(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "lt");
    Opcode op = (type == Type::I64) ? Opcode::LtI64 : Opcode::LtU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_le(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "le");
    Opcode op = (type == Type::I64) ? Opcode::LeI64 : Opcode::LeU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_gt(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "gt");
    Opcode op = (type == Type::I64) ? Opcode::GtI64 : Opcode::GtU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_ge(ValueId lhs, ValueId rhs, Type type) {
    ValueId result = function_->create_value(Type::I1, "ge");
    Opcode op = (type == Type::I64) ? Opcode::GeI64 : Opcode::GeU64;
    auto instr = std::make_unique<CompareOp>(op, lhs, rhs, result);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_and(ValueId lhs, ValueId rhs) {
    ValueId result = function_->create_value(Type::I1, "and");
    auto instr = std::make_unique<BinaryOp>(Opcode::And, lhs, rhs, result, Type::I1);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_or(ValueId lhs, ValueId rhs) {
    ValueId result = function_->create_value(Type::I1, "or");
    auto instr = std::make_unique<BinaryOp>(Opcode::Or, lhs, rhs, result, Type::I1);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_not(ValueId operand) {
    ValueId result = function_->create_value(Type::I1, "not");
    auto instr = std::make_unique<UnaryOp>(Opcode::Not, operand, result, Type::I1);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_bitand(ValueId lhs, ValueId rhs) {
    ValueId result = function_->create_value(Type::U64, "bitand");
    auto instr = std::make_unique<BinaryOp>(Opcode::BitAnd, lhs, rhs, result, Type::U64);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_bitor(ValueId lhs, ValueId rhs) {
    ValueId result = function_->create_value(Type::U64, "bitor");
    auto instr = std::make_unique<BinaryOp>(Opcode::BitOr, lhs, rhs, result, Type::U64);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_bitxor(ValueId lhs, ValueId rhs) {
    ValueId result = function_->create_value(Type::U64, "bitxor");
    auto instr = std::make_unique<BinaryOp>(Opcode::BitXor, lhs, rhs, result, Type::U64);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_bitnot(ValueId operand) {
    ValueId result = function_->create_value(Type::U64, "bitnot");
    auto instr = std::make_unique<UnaryOp>(Opcode::BitNot, operand, result, Type::U64);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_select(ValueId cond, ValueId true_val, ValueId false_val, Type type) {
    ValueId result = function_->create_value(type, "select");
    auto instr = std::make_unique<SelectOp>(cond, true_val, false_val, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_load_column(u32 column_id, ValueId row_idx, Type type) {
    ValueId result = function_->create_value(type, "load");
    auto instr = std::make_unique<LoadColumnOp>(column_id, row_idx, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_count(ValueId input) {
    ValueId result = function_->create_value(Type::U64, "count");
    auto instr = std::make_unique<AggregateOp>(Opcode::Count, input, result, Type::U64);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_sum(ValueId input, Type type) {
    ValueId result = function_->create_value(type, "sum");
    Opcode op = (type == Type::I64) ? Opcode::SumI64 : Opcode::SumU64;
    auto instr = std::make_unique<AggregateOp>(op, input, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_min(ValueId input, Type type) {
    ValueId result = function_->create_value(type, "min");
    Opcode op = (type == Type::I64) ? Opcode::MinI64 : Opcode::MinU64;
    auto instr = std::make_unique<AggregateOp>(op, input, result, type);
    current_block_->append(std::move(instr));
    return result;
}

ValueId Builder::create_max(ValueId input, Type type) {
    ValueId result = function_->create_value(type, "max");
    Opcode op = (type == Type::I64) ? Opcode::MaxI64 : Opcode::MaxU64;
    auto instr = std::make_unique<AggregateOp>(op, input, result, type);
    current_block_->append(std::move(instr));
    return result;
}

void Builder::create_return(ValueId value) {
    auto instr = std::make_unique<ReturnOp>(value);
    current_block_->append(std::move(instr));
}

void Builder::create_jump(BasicBlock* target) {
    auto instr = std::make_unique<JumpOp>(target);
    current_block_->append(std::move(instr));
}

void Builder::create_branch(ValueId cond, BasicBlock* true_block, BasicBlock* false_block) {
    auto instr = std::make_unique<BranchOp>(cond, true_block, false_block);
    current_block_->append(std::move(instr));
}

ValueId Builder::lower_expression(const query::Expression* expr) {
    if (!expr) return 0;
    
    switch (expr->type) {
        case query::ExprType::Literal: {
            auto lit = static_cast<const query::LiteralExpr*>(expr);
            return create_const_u64(lit->value);
        }
        
        case query::ExprType::ColumnRef: {
            auto ref = static_cast<const query::ColumnRefExpr*>(expr);
            // Map column name to column ID
            u32 column_id = 0;  // TODO: proper column resolution
            ValueId row_idx = function_->parameters()[0];  // First param is row index
            return create_load_column(column_id, row_idx, Type::U64);
        }
        
        case query::ExprType::BinaryOp: {
            auto binop = static_cast<const query::BinaryOpExpr*>(expr);
            ValueId lhs = lower_expression(binop->left.get());
            ValueId rhs = lower_expression(binop->right.get());
            Type type = Type::U64;  // TODO: proper type inference
            
            switch (binop->op) {
                case query::TokenType::Plus: return create_add(lhs, rhs, type);
                case query::TokenType::Minus: return create_sub(lhs, rhs, type);
                case query::TokenType::Star: return create_mul(lhs, rhs, type);
                case query::TokenType::Slash: return create_div(lhs, rhs, type);
                case query::TokenType::Percent: return create_mod(lhs, rhs, type);
                case query::TokenType::Equal: return create_eq(lhs, rhs, type);
                case query::TokenType::NotEqual: return create_ne(lhs, rhs, type);
                case query::TokenType::Less: return create_lt(lhs, rhs, type);
                case query::TokenType::LessEqual: return create_le(lhs, rhs, type);
                case query::TokenType::Greater: return create_gt(lhs, rhs, type);
                case query::TokenType::GreaterEqual: return create_ge(lhs, rhs, type);
                case query::TokenType::And: return create_and(lhs, rhs);
                case query::TokenType::Or: return create_or(lhs, rhs);
                case query::TokenType::Amp: return create_bitand(lhs, rhs);
                case query::TokenType::Pipe: return create_bitor(lhs, rhs);
                case query::TokenType::Caret: return create_bitxor(lhs, rhs);
                default: return 0;
            }
        }
        
        case query::ExprType::UnaryOp: {
            auto unop = static_cast<const query::UnaryOpExpr*>(expr);
            ValueId operand = lower_expression(unop->operand.get());
            
            switch (unop->op) {
                case query::TokenType::Minus: return create_neg(operand, Type::I64);
                case query::TokenType::Bang: return create_not(operand);
                default: return 0;
            }
        }
        
        default:
            return 0;
    }
}

Type Builder::infer_type(const query::Expression* expr) const {
    // TODO: implement proper type inference
    return Type::U64;
}

Opcode Builder::get_comparison_opcode(query::TokenType op, Type type) const {
    // TODO: implement
    return Opcode::EqU64;
}

Opcode Builder::get_arithmetic_opcode(query::TokenType op, Type type) const {
    // TODO: implement
    return Opcode::AddU64;
}

} // namespace ir
} // namespace vectortick
