#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>

//  Recursive-descent parser

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> toks_;
    size_t             pos_ = 0;

    // Token helpers
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool         check(TK t) const;
    bool         match(TK t);
    const Token& expect(TK t, const std::string& msg);

    // Grammar rules
    FuncDef              parseFuncDef();
    std::vector<StmtPtr> parseBlock();
    StmtPtr              parseStmt();
    StmtPtr              parseVarDecl();
    StmtPtr              parseIfStmt();
    StmtPtr              parseWhileStmt();
    std::vector<StmtPtr> parseForStmt();   // desugars into init + while
    StmtPtr              parseReturn();
    StmtPtr              parsePrint();

    ExprPtr parseExpr();
    ExprPtr parseOrExpr();
    ExprPtr parseAndExpr();
    ExprPtr parseCmpExpr();
    ExprPtr parseAddExpr();
    ExprPtr parseMulExpr();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
};
