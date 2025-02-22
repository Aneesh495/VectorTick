#include "vectortick/query/lexer.hpp"
#include <cctype>

namespace vectortick {
namespace query {

const char* token_type_name(TokenType type) noexcept {
    switch (type) {
        case TokenType::Eof: return "EOF";
        case TokenType::IntegerLiteral: return "IntegerLiteral";
        case TokenType::Identifier: return "Identifier";
        case TokenType::From: return "FROM";
        case TokenType::Where: return "WHERE";
        case TokenType::Let: return "LET";
        case TokenType::Group: return "GROUP";
        case TokenType::By: return "BY";
        case TokenType::Agg: return "AGG";
        case TokenType::Order: return "ORDER";
        case TokenType::Limit: return "LIMIT";
        case TokenType::And: return "AND";
        case TokenType::Or: return "OR";
        case TokenType::Not: return "NOT";
        case TokenType::True: return "TRUE";
        case TokenType::False: return "FALSE";
        case TokenType::Tumble: return "TUMBLE";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        case TokenType::Equal: return "==";
        case TokenType::NotEqual: return "!=";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        case TokenType::AmpAmp: return "&&";
        case TokenType::PipePipe: return "||";
        case TokenType::Bang: return "!";
        case TokenType::Amp: return "&";
        case TokenType::Pipe: return "|";
        case TokenType::Caret: return "^";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::Comma: return ",";
        case TokenType::Semicolon: return ";";
        case TokenType::Error: return "Error";
        default: return "Unknown";
    }
}

Token Lexer::next_token() noexcept {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_token_;
    }
    
    skip_whitespace();
    
    if (at_end()) {
        return make_token(TokenType::Eof);
    }
    
    char c = current();
    
    // Comments
    if (c == '/' && peek() == '/') {
        skip_line_comment();
        return next_token();
    }
    
    // Numbers
    if (std::isdigit(c)) {
        return read_number();
    }
    
    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return read_identifier();
    }
    
    // Operators and punctuation
    return read_operator();
}

Token Lexer::peek_token() noexcept {
    if (!has_peeked_) {
        peeked_token_ = next_token();
        has_peeked_ = true;
    }
    return peeked_token_;
}

void Lexer::skip_whitespace() noexcept {
    while (!at_end()) {
        char c = current();
        if (std::isspace(c)) {
            advance();
        } else if (c == '/' && peek() == '/') {
            skip_line_comment();
        } else {
            break;
        }
    }
}

void Lexer::skip_line_comment() noexcept {
    while (!at_end() && current() != '\n') {
        advance();
    }
}

Token Lexer::read_number() noexcept {
    usize start = pos_;
    usize start_line = line_;
    usize start_col = column_;
    
    while (!at_end() && std::isdigit(current())) {
        advance();
    }
    
    std::string_view text = input_.substr(start, pos_ - start);
    
    // Parse integer value
    u64 value = 0;
    for (char c : text) {
        if (value > (UINT64_MAX - (c - '0')) / 10) {
            return error_token("Integer literal too large");
        }
        value = value * 10 + (c - '0');
    }
    
    return Token(TokenType::IntegerLiteral, text, value, start_line, start_col);
}

Token Lexer::read_identifier() noexcept {
    usize start = pos_;
    usize start_line = line_;
    usize start_col = column_;
    
    while (!at_end() && (std::isalnum(current()) || current() == '_')) {
        advance();
    }
    
    std::string_view text = input_.substr(start, pos_ - start);
    TokenType type = lookup_keyword(text);
    
    if (type == TokenType::True) {
        return Token(TokenType::True, text, 1, start_line, start_col);
    } else if (type == TokenType::False) {
        return Token(TokenType::False, text, 0, start_line, start_col);
    }
    
    return Token(type, text, start_line, start_col);
}

Token Lexer::read_operator() noexcept {
    usize start_line = line_;
    usize start_col = column_;
    char c = current();
    
    switch (c) {
        case '+': advance(); return make_token(TokenType::Plus);
        case '-': advance(); return make_token(TokenType::Minus);
        case '*': advance(); return make_token(TokenType::Star);
        case '/': advance(); return make_token(TokenType::Slash);
        case '%': advance(); return make_token(TokenType::Percent);
        case '(': advance(); return make_token(TokenType::LParen);
        case ')': advance(); return make_token(TokenType::RParen);
        case ',': advance(); return make_token(TokenType::Comma);
        case ';': advance(); return make_token(TokenType::Semicolon);
        
        case '=':
            advance();
            if (current() == '=') {
                advance();
                return make_token(TokenType::Equal);
            }
            return error_token("Expected '=='");
        
        case '!':
            advance();
            if (current() == '=') {
                advance();
                return make_token(TokenType::NotEqual);
            }
            return make_token(TokenType::Bang);
        
        case '<':
            advance();
            if (current() == '=') {
                advance();
                return make_token(TokenType::LessEqual);
            }
            return make_token(TokenType::Less);
        
        case '>':
            advance();
            if (current() == '=') {
                advance();
                return make_token(TokenType::GreaterEqual);
            }
            return make_token(TokenType::Greater);
        
        case '&':
            advance();
            if (current() == '&') {
                advance();
                return make_token(TokenType::AmpAmp);
            }
            return make_token(TokenType::Amp);
        
        case '|':
            advance();
            if (current() == '|') {
                advance();
                return make_token(TokenType::PipePipe);
            }
            return make_token(TokenType::Pipe);
        
        case '^':
            advance();
            return make_token(TokenType::Caret);
        
        default:
            advance();
            return error_token("Unexpected character");
    }
}

TokenType Lexer::lookup_keyword(std::string_view ident) noexcept {
    if (ident == "FROM") return TokenType::From;
    if (ident == "WHERE") return TokenType::Where;
    if (ident == "LET") return TokenType::Let;
    if (ident == "GROUP") return TokenType::Group;
    if (ident == "BY") return TokenType::By;
    if (ident == "AGG") return TokenType::Agg;
    if (ident == "ORDER") return TokenType::Order;
    if (ident == "LIMIT") return TokenType::Limit;
    if (ident == "AND") return TokenType::And;
    if (ident == "OR") return TokenType::Or;
    if (ident == "NOT") return TokenType::Not;
    if (ident == "TRUE") return TokenType::True;
    if (ident == "FALSE") return TokenType::False;
    if (ident == "TUMBLE") return TokenType::Tumble;
    
    return TokenType::Identifier;
}

Token Lexer::make_token(TokenType type) noexcept {
    return Token(type, input_.substr(pos_ - 1, 1), line_, column_ - 1);
}

Token Lexer::error_token(std::string_view msg) noexcept {
    has_error_ = true;
    error_msg_ = msg;
    return Token(TokenType::Error, msg, line_, column_);
}

} // namespace query
} // namespace vectortick
