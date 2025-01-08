#pragma once

#include "../common/types.hpp"
#include <string_view>

namespace vectortick {

namespace query {

// Token types for the query language
enum class TokenType : u8 {
    // End of input
    Eof = 0,
    
    // Literals
    IntegerLiteral,
    Identifier,
    
    // Keywords
    From,
    Where,
    Let,
    Group,
    By,
    Agg,
    Order,
    Limit,
    And,
    Or,
    Not,
    True,
    False,
    Tumble,
    
    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    Equal,          // ==
    NotEqual,       // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=
    AmpAmp,         // &&
    PipePipe,       // ||
    Bang,           // !
    Amp,            // &
    Pipe,           // |
    Caret,          // ^
    
    // Punctuation
    LParen,         // (
    RParen,         // )
    Comma,          // ,
    Semicolon,      // ;
    
    // Error
    Error
};

// Token structure
struct Token {
    TokenType type;
    std::string_view text;
    u64 int_value;
    usize line;
    usize column;
    
    Token() : type(TokenType::Eof), int_value(0), line(0), column(0) {}
    
    Token(TokenType t, std::string_view txt, usize ln, usize col)
        : type(t), text(txt), int_value(0), line(ln), column(col) {}
    
    Token(TokenType t, std::string_view txt, u64 val, usize ln, usize col)
        : type(t), text(txt), int_value(val), line(ln), column(col) {}
    
    [[nodiscard]] bool is_keyword() const noexcept {
        return type >= TokenType::From && type <= TokenType::Tumble;
    }
    
    [[nodiscard]] bool is_operator() const noexcept {
        return type >= TokenType::Plus && type <= TokenType::Caret;
    }
    
    [[nodiscard]] bool is_literal() const noexcept {
        return type == TokenType::IntegerLiteral || 
               type == TokenType::True || 
               type == TokenType::False;
    }
};

// Get token type name
[[nodiscard]] const char* token_type_name(TokenType type) noexcept;

} // namespace query

} // namespace vectortick
