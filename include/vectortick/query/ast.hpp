#pragma once

#include "token.hpp"
#include "../common/types.hpp"
#include <memory>
#include <vector>
#include <string>

namespace vectortick {

namespace query {

// Forward declarations
struct Expression;
struct Statement;

// Expression types
enum class ExprType : u8 {
    Literal,        // Integer constant
    ColumnRef,      // Column reference
    BinaryOp,       // Binary operation
    UnaryOp,        // Unary operation
    FunctionCall,   // Function call
    Select          // Conditional select (cond ? a : b equivalent)
};

// AST node for expressions
struct Expression {
    ExprType type;
    usize line;
    usize column;
    
    Expression(ExprType t, usize ln, usize col) : type(t), line(ln), column(col) {}
    virtual ~Expression() = default;
};

// Integer literal
struct LiteralExpr : Expression {
    u64 value;
    
    LiteralExpr(u64 val, usize ln, usize col)
        : Expression(ExprType::Literal, ln, col), value(val) {}
};

// Column reference
struct ColumnRefExpr : Expression {
    std::string name;
    
    ColumnRefExpr(const std::string& n, usize ln, usize col)
        : Expression(ExprType::ColumnRef, ln, col), name(n) {}
};

// Binary operation
struct BinaryOpExpr : Expression {
    TokenType op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    
    BinaryOpExpr(TokenType o, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r, usize ln, usize col)
        : Expression(ExprType::BinaryOp, ln, col), op(o), left(std::move(l)), right(std::move(r)) {}
};

// Unary operation
struct UnaryOpExpr : Expression {
    TokenType op;
    std::unique_ptr<Expression> operand;
    
    UnaryOpExpr(TokenType o, std::unique_ptr<Expression> opnd, usize ln, usize col)
        : Expression(ExprType::UnaryOp, ln, col), op(o), operand(std::move(opnd)) {}
};

// Function call (e.g., count(), sum())
struct FunctionCallExpr : Expression {
    std::string name;
    std::vector<std::unique_ptr<Expression>> args;
    
    FunctionCallExpr(const std::string& n, usize ln, usize col)
        : Expression(ExprType::FunctionCall, ln, col), name(n) {}
};

// Statement types
enum class StmtType : u8 {
    Query,          // Full query statement
    SelectClause,   // SELECT/AGG clause
    WhereClause,    // WHERE clause
    LetClause,      // LET clause
    GroupByClause,  // GROUP BY clause
    OrderByClause,  // ORDER BY clause
    LimitClause     // LIMIT clause
};

// Base statement
struct Statement {
    StmtType type;
    usize line;
    usize column;
    
    Statement(StmtType t, usize ln, usize col) : type(t), line(ln), column(col) {}
    virtual ~Statement() = default;
};

// Complete query
struct QueryStmt : Statement {
    std::string table_name;                                 // FROM table
    std::unique_ptr<Expression> where_expr;                // WHERE condition
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> let_bindings; // LET clauses
    std::vector<std::string> group_by_columns;             // GROUP BY columns
    std::unique_ptr<Expression> tumble_window;             // TUMBLE(...)
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> aggregations; // AGG clauses
    std::vector<std::pair<std::string, bool>> order_by;    // ORDER BY (column, ascending)
    u64 limit;                                              // LIMIT
    
    QueryStmt(usize ln, usize col)
        : Statement(StmtType::Query, ln, col), limit(0) {}
};

} // namespace query

} // namespace vectortick
