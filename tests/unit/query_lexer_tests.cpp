#include "../test_framework.hpp"
#include "vectortick/query/lexer.hpp"

using namespace vectortick;
using namespace vectortick::query;

VT_TEST(query_lexer_tests, tokenize_sql_keywords_case_insensitive) {
    std::string sql = "select FROM where GROUP by ORDER BY limit as asc desc count sum min max avg";
    Lexer lexer(sql);
    
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Select));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::From));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Where));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Group));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::By));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Order));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::By));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Limit));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::As));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Asc));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Desc));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Count));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Sum));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Min));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Max));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Avg));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Eof));
}

VT_TEST(query_lexer_tests, operators_and_punctuation) {
    std::string text = "+ - * / % = == != < <= > >= && || ! & | ^ ( ) , ;";
    Lexer lexer(text);
    
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Plus));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Minus));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Star));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Slash));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Percent));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Equal));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Equal));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::NotEqual));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Less));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::LessEqual));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Greater));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::GreaterEqual));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::AmpAmp));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::PipePipe));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Bang));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Amp));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Pipe));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Caret));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::LParen));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::RParen));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Comma));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Semicolon));
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Eof));
}

VT_TEST(query_lexer_tests, literals_and_comments) {
    std::string text = "12345 // comment line\n true FALSE price_ticks";
    Lexer lexer(text);
    
    Token num = lexer.next_token();
    VT_ASSERT_EQ(static_cast<int>(num.type), static_cast<int>(TokenType::IntegerLiteral));
    VT_ASSERT_EQ(num.int_value, 12345ULL);
    
    Token t = lexer.next_token();
    VT_ASSERT_EQ(static_cast<int>(t.type), static_cast<int>(TokenType::True));
    
    Token f = lexer.next_token();
    VT_ASSERT_EQ(static_cast<int>(f.type), static_cast<int>(TokenType::False));
    
    Token id = lexer.next_token();
    VT_ASSERT_EQ(static_cast<int>(id.type), static_cast<int>(TokenType::Identifier));
    VT_ASSERT(id.text == "price_ticks");
    
    VT_ASSERT_EQ(static_cast<int>(lexer.next_token().type), static_cast<int>(TokenType::Eof));
}
