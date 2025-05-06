#include "vectortick/query/type_checker.hpp"
#include <cctype>

namespace vectortick {
namespace query {

namespace {
inline bool ieq(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}
} // namespace

bool TypeChecker::resolve_column(std::string_view name, vts1::ColumnID& out_id) noexcept {
    if (ieq(name, "exchange_ts_ns") || ieq(name, "exchange_ts")) {
        out_id = vts1::ColumnID::ExchangeTsNs; return true;
    } else if (ieq(name, "receive_ts_ns") || ieq(name, "receive_ts")) {
        out_id = vts1::ColumnID::ReceiveTsNs; return true;
    } else if (ieq(name, "sequence") || ieq(name, "seq")) {
        out_id = vts1::ColumnID::Sequence; return true;
    } else if (ieq(name, "instrument_id") || ieq(name, "symbol") || ieq(name, "instrument")) {
        out_id = vts1::ColumnID::InstrumentId; return true;
    } else if (ieq(name, "event_type")) {
        out_id = vts1::ColumnID::EventType; return true;
    } else if (ieq(name, "side")) {
        out_id = vts1::ColumnID::Side; return true;
    } else if (ieq(name, "flags")) {
        out_id = vts1::ColumnID::Flags; return true;
    } else if (ieq(name, "price_ticks") || ieq(name, "price")) {
        out_id = vts1::ColumnID::PriceTicks; return true;
    } else if (ieq(name, "quantity") || ieq(name, "qty")) {
        out_id = vts1::ColumnID::Quantity; return true;
    } else if (ieq(name, "venue_id") || ieq(name, "venue")) {
        out_id = vts1::ColumnID::VenueId; return true;
    } else if (ieq(name, "source_id") || ieq(name, "source")) {
        out_id = vts1::ColumnID::SourceId; return true;
    } else if (ieq(name, "trade_or_order_id") || ieq(name, "trade_id") || ieq(name, "order_id")) {
        out_id = vts1::ColumnID::TradeOrOrderId; return true;
    }
    return false;
}

Result<vts1::ColumnType> TypeChecker::check_expression(const Expression* expr) const noexcept {
    if (!expr) {
        return make_error<vts1::ColumnType>(StatusCode::InvalidArgument, "Null expression");
    }
    
    switch (expr->type) {
        case ExprType::Literal:
            return vts1::ColumnType::U64;
            
        case ExprType::ColumnRef: {
            auto col = static_cast<const ColumnRefExpr*>(expr);
            vts1::ColumnID id;
            if (!resolve_column(col->name, id)) {
                return make_error<vts1::ColumnType>(StatusCode::InvalidArgument, "Unknown column reference: " + col->name);
            }
            return vts1::get_column_type(id);
        }
        
        case ExprType::BinaryOp: {
            auto binop = static_cast<const BinaryOpExpr*>(expr);
            auto left_res = check_expression(binop->left.get());
            if (!left_res.ok()) return left_res;
            auto right_res = check_expression(binop->right.get());
            if (!right_res.ok()) return right_res;
            
            switch (binop->op) {
                case TokenType::Equal:
                case TokenType::NotEqual:
                case TokenType::Less:
                case TokenType::LessEqual:
                case TokenType::Greater:
                case TokenType::GreaterEqual:
                case TokenType::And:
                case TokenType::Or:
                    return vts1::ColumnType::Bool;
                    
                default:
                    if (left_res.value() == vts1::ColumnType::I64 || right_res.value() == vts1::ColumnType::I64) {
                        return vts1::ColumnType::I64;
                    }
                    return vts1::ColumnType::U64;
            }
        }
        
        case ExprType::UnaryOp: {
            auto unop = static_cast<const UnaryOpExpr*>(expr);
            auto operand_res = check_expression(unop->operand.get());
            if (!operand_res.ok()) return operand_res;
            
            if (unop->op == TokenType::Bang) {
                return vts1::ColumnType::Bool;
            } else if (unop->op == TokenType::Minus) {
                return vts1::ColumnType::I64;
            }
            return operand_res;
        }
        
        case ExprType::FunctionCall: {
            auto fn = static_cast<const FunctionCallExpr*>(expr);
            for (const auto& arg : fn->args) {
                auto arg_res = check_expression(arg.get());
                if (!arg_res.ok()) return arg_res;
            }
            if (ieq(fn->name, "count")) {
                return vts1::ColumnType::U64;
            } else if (ieq(fn->name, "sum") || ieq(fn->name, "min") || ieq(fn->name, "max")) {
                if (!fn->args.empty()) {
                    auto a = check_expression(fn->args[0].get());
                    if (a.ok()) return a.value();
                }
                return vts1::ColumnType::U64;
            }
            return vts1::ColumnType::U64;
        }
        
        default:
            return vts1::ColumnType::U64;
    }
}

Status TypeChecker::check_query(const QueryStmt* stmt) const noexcept {
    if (!stmt) {
        return Status(StatusCode::InvalidArgument, "Null query statement");
    }
    
    // Check projections
    for (const auto& proj : stmt->projections) {
        if (!proj.is_wildcard && proj.expr) {
            auto res = check_expression(proj.expr.get());
            if (!res.ok()) return res.status();
        }
    }
    
    // Check WHERE condition
    if (stmt->where_expr) {
        auto res = check_expression(stmt->where_expr.get());
        if (!res.ok()) return res.status();
    }
    
    // Check GROUP BY columns
    for (const auto& col_name : stmt->group_by_columns) {
        vts1::ColumnID id;
        if (!resolve_column(col_name, id)) {
            return Status(StatusCode::InvalidArgument, "Unknown GROUP BY column: " + col_name);
        }
    }
    
    // Check ORDER BY columns
    for (const auto& order_pair : stmt->order_by) {
        vts1::ColumnID id;
        if (!resolve_column(order_pair.first, id)) {
            return Status(StatusCode::InvalidArgument, "Unknown ORDER BY column: " + order_pair.first);
        }
    }
    
    return Status::OK();
}

} // namespace query
} // namespace vectortick
