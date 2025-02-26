#pragma once

#include "token.hpp"
#include "lexer.hpp"
#include "ast.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include <memory>
#include <string_view>

namespace vectortick {

namespace query {

// Recursive descent parser for query language
class Parser {
public:
    explicit Parser(std::string_view input) noexcept
        : lexer_(input), has_error_(false) {}
    
    // Parse complete query
    [[nodiscard]] Result<std::unique_ptr<QueryStmt>> parse_query() noexcept;
    
    // Check if has error
    [[nodiscard]] bool has_error() const noexcept { return has_error_; }
    
    // Get error message
    [[nodiscard]] std::string_view error_message() const noexcept { return error_msg_; }
    
    // Get error location
    [[nodiscard]] usize error_line() const noexcept { return error_line_; }
    [[nodiscard]] usize error_column() const noexcept { return error_column_; }

private:
    // Token consumption
    [[nodiscard]] Token current_token() noexcept { return lexer_.peek_token(); }
    Token advance() noexcept { return lexer_.next_token(); }
    
    [[nodiscard]] bool check(TokenType type) noexcept {
        return current_token().type == type;
    }
    
    [[nodiscard]] bool match(TokenType type) noexcept {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }
    
    [[nodiscard]] Token expect(TokenType type, const char* msg) noexcept;
    
    // Parsing methods
    [[nodiscard]] std::unique_ptr<Expression> parse_expression() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_or_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_and_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_bitwise_or_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_bitwise_xor_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_bitwise_and_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_equality_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_comparison_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_additive_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_multiplicative_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_unary_expr() noexcept;
    [[nodiscard]] std::unique_ptr<Expression> parse_primary_expr() noexcept;
    
    [[nodiscard]] std::unique_ptr<Expression> parse_function_call(const std::string& name, usize line, usize col) noexcept;
    
    void set_error(const char* msg, usize line, usize col) noexcept;
    
    Lexer lexer_;
    bool has_error_;
    std::string error_msg_;
    usize error_line_;
    usize error_column_;
};

} // namespace query

} // namespace vectortick
