#pragma once
#include <string>
#include <vector>
#include <stdexcept>

enum class TK {
    // Literals / names
    INT_LIT, IDENT,
    // Keywords
    KW_FUNC, KW_VAR, KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_BREAK, KW_CONTINUE, KW_RETURN, KW_PRINT,
    // Arithmetic
    PLUS, MINUS, STAR, SLASH, PERCENT,
    // Relational
    EQ, NEQ, LT, GT, LEQ, GEQ,
    // Logical
    AND, OR, BANG,
    // Assignment
    ASSIGN,
    // Punctuation
    LPAREN, RPAREN, LBRACE, RBRACE, SEMICOLON, COMMA,
    // Sentinel
    END
};

struct Token {
    TK      type;
    std::string value;   // raw text (used for IDENT and INT_LIT)
    int     line;
};

class Lexer {
public:
    explicit Lexer(std::string src) : src_(std::move(src)) {}
    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t pos_  = 0;
    int    line_ = 1;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespaceAndComments();
    Token scanNumber();
    Token scanIdent();
};
