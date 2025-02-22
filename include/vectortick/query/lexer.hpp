#pragma once

#include "token.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include <string_view>

namespace vectortick {

namespace query {

// Lexer for the query language
class Lexer {
public:
    explicit Lexer(std::string_view input) noexcept
        : input_(input)
        , pos_(0)
        , line_(1)
        , column_(1)
        , has_error_(false) {}
    
    // Get next token
    [[nodiscard]] Token next_token() noexcept;
    
    // Peek at next token without consuming
    [[nodiscard]] Token peek_token() noexcept;
    
    // Get current position
    [[nodiscard]] usize position() const noexcept { return pos_; }
    [[nodiscard]] usize line() const noexcept { return line_; }
    [[nodiscard]] usize column() const noexcept { return column_; }
    
    // Check if at end
    [[nodiscard]] bool at_end() const noexcept { return pos_ >= input_.size(); }
    
    // Check if has error
    [[nodiscard]] bool has_error() const noexcept { return has_error_; }
    
    // Get error message
    [[nodiscard]] std::string_view error_message() const noexcept { return error_msg_; }

private:
    [[nodiscard]] char current() const noexcept {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }
    
    [[nodiscard]] char peek(usize offset = 1) const noexcept {
        usize idx = pos_ + offset;
        return idx < input_.size() ? input_[idx] : '\0';
    }
    
    char advance() noexcept {
        char c = current();
        ++pos_;
        if (c == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return c;
    }
    
    void skip_whitespace() noexcept;
    void skip_line_comment() noexcept;
    
    [[nodiscard]] Token read_number() noexcept;
    [[nodiscard]] Token read_identifier() noexcept;
    [[nodiscard]] Token read_operator() noexcept;
    
    [[nodiscard]] Token make_token(TokenType type) noexcept;
    [[nodiscard]] Token error_token(std::string_view msg) noexcept;
    
    // Check if identifier is a keyword
    [[nodiscard]] static TokenType lookup_keyword(std::string_view ident) noexcept;
    
    std::string_view input_;
    usize pos_;
    usize line_;
    usize column_;
    
    bool has_error_;
    std::string_view error_msg_;
    
    // For peeking
    Token peeked_token_;
    bool has_peeked_ = false;
};

} // namespace query

} // namespace vectortick
