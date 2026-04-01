#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TK> KEYWORDS = {
    {"func", TK::KW_FUNC},
    {"var", TK::KW_VAR},
    {"if", TK::KW_IF},
    {"else", TK::KW_ELSE},
    {"while", TK::KW_WHILE},
    {"for", TK::KW_FOR},
    {"return", TK::KW_RETURN},
    {"print", TK::KW_PRINT},
};

char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    return (idx < src_.size()) ? src_[idx] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') ++line_;
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos_ < src_.size()) {
        char c = peek();
        if (std::isspace(c)) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Line comment — consume until newline
            while (pos_ < src_.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            // Block comment
            advance(); advance(); // consume /*
            while (pos_ < src_.size()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance(); break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::scanNumber() {
    std::string s;
    while (pos_ < src_.size() && std::isdigit(peek()))
        s += advance();
    return {TK::INT_LIT, s, line_};
}

Token Lexer::scanIdent() {
    std::string s;
    while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
        s += advance();
    auto it = KEYWORDS.find(s);
    TK tk = (it != KEYWORDS.end()) ? it->second : TK::IDENT;
    return {tk, s, line_};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (pos_ >= src_.size()) break;

        char c = peek();
        int ln = line_;

        // Numbers
        if (std::isdigit(c)) { tokens.push_back(scanNumber()); continue; }
        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') { tokens.push_back(scanIdent()); continue; }

        // Operators & punctuation
        advance(); // consume current char
        switch (c) {
            case '+': tokens.push_back({TK::PLUS,      "+", ln}); break;
            case '-': tokens.push_back({TK::MINUS,     "-", ln}); break;
            case '*': tokens.push_back({TK::STAR,      "*", ln}); break;
            case '/': tokens.push_back({TK::SLASH,     "/", ln}); break;
            case '%': tokens.push_back({TK::PERCENT,   "%", ln}); break;
            case '(': tokens.push_back({TK::LPAREN,    "(", ln}); break;
            case ')': tokens.push_back({TK::RPAREN,    ")", ln}); break;
            case '{': tokens.push_back({TK::LBRACE,    "{", ln}); break;
            case '}': tokens.push_back({TK::RBRACE,    "}", ln}); break;
            case ';': tokens.push_back({TK::SEMICOLON, ";", ln}); break;
            case ',': tokens.push_back({TK::COMMA,     ",", ln}); break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back({TK::NEQ, "!=", ln}); }
                else { tokens.push_back({TK::BANG, "!", ln}); }
                break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back({TK::EQ,     "==", ln}); }
                else { tokens.push_back({TK::ASSIGN, "=",  ln}); }
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back({TK::LEQ, "<=", ln}); }
                else { tokens.push_back({TK::LT,  "<",  ln}); }
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back({TK::GEQ, ">=", ln}); }
                else { tokens.push_back({TK::GT,  ">",  ln}); }
                break;
            case '&':
                if (peek() == '&') { advance(); tokens.push_back({TK::AND, "&&", ln}); }
                else throw std::runtime_error("Unexpected '&' at line " + std::to_string(ln));
                break;
            case '|':
                if (peek() == '|') { advance(); tokens.push_back({TK::OR, "||", ln}); }
                else throw std::runtime_error("Unexpected '|' at line " + std::to_string(ln));
                break;
            default:
                throw std::runtime_error(std::string("Unknown character '") + c + "' at line " + std::to_string(ln));
        }
    }
    tokens.push_back({TK::END, "", line_});
    return tokens;
}
