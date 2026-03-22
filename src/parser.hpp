#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>

// ─────────────────────────────────────────────────────────────
//  Recursive-descent parser
//
//  Grammar (informal):
//    program  := funcdef*
//    funcdef  := 'func' IDENT '(' params ')' block
//    params   := (IDENT (',' IDENT)*)?
//    block    := '{' stmt* '}'
//    stmt     := vardecl | assign | ifstmt | whilestmt
//               | return | print | exprstmt
//    vardecl  := 'var' IDENT '=' expr ';'
//    assign   := IDENT '=' expr ';'
//    ifstmt   := 'if' '(' expr ')' block ('else' block)?
//    while    := 'while' '(' expr ')' block
//    return   := 'return' expr ';'
//    print    := 'print' '(' expr ')' ';'
//    exprstmt := expr ';'
//    expr     := orExpr
//    orExpr   := andExpr ('||' andExpr)*
//    andExpr  := cmpExpr ('&&' cmpExpr)*
//    cmpExpr  := addExpr (('=='|'!='|'<'|'>'|'<='|'>=') addExpr)?
//    addExpr  := mulExpr (('+' | '-') mulExpr)*
//    mulExpr  := unary  (('*' | '/' | '%') unary)*
//    unary    := '-' unary | '!' unary | primary
//    primary  := INT_LIT | IDENT | IDENT '(' args ')' | '(' expr ')'
//    args     := (expr (',' expr)*)?
// ─────────────────────────────────────────────────────────────

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
