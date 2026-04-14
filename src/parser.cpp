#include "parser.hpp"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : toks_(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= toks_.size()) return toks_.back(); // END token
    return toks_[idx];
}

const Token& Parser::advance() {
    const Token& t = toks_[pos_];
    if (t.type != TK::END) ++pos_;
    return t;
}

bool Parser::check(TK t) const { return peek().type == t; }

bool Parser::match(TK t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TK t, const std::string& msg) {
    if (!check(t))
        throw std::runtime_error(msg + " (got '" + peek().value + "' at line " + std::to_string(peek().line) + ")");
    return advance();
}

Program Parser::parse() {
    Program prog;
    while (!check(TK::END))
        prog.funcs.push_back(parseFuncDef());
    return prog;
}

FuncDef Parser::parseFuncDef() {
    FuncDef fn;
    fn.line = peek().line;
    expect(TK::KW_FUNC, "Expected 'func'");
    fn.name = expect(TK::IDENT, "Expected function name").value;
    expect(TK::LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check(TK::RPAREN)) {
        fn.params.push_back(expect(TK::IDENT, "Expected parameter name").value);
        while (match(TK::COMMA))
            fn.params.push_back(expect(TK::IDENT, "Expected parameter name").value);
    }
    expect(TK::RPAREN, "Expected ')' after parameters");

    fn.body = parseBlock();
    return fn;
}

std::vector<StmtPtr> Parser::parseBlock() {
    expect(TK::LBRACE, "Expected '{'");
    std::vector<StmtPtr> stmts;
    while (!check(TK::RBRACE) && !check(TK::END)) {
        if (check(TK::KW_FOR)) {
            // for desugars into multiple stmts (init + while); splice them in
            auto forStmts = parseForStmt();
            for (auto& s : forStmts)
                stmts.push_back(std::move(s));
        } else {
            stmts.push_back(parseStmt());
        }
    }
    expect(TK::RBRACE, "Expected '}'");
    return stmts;
}

StmtPtr Parser::parseStmt() {
    int ln = peek().line;
    switch (peek().type) {
        case TK::KW_VAR:    return parseVarDecl();
        case TK::KW_IF:     return parseIfStmt();
        case TK::KW_WHILE:  return parseWhileStmt();
        case TK::KW_BREAK:  return parseBreak();
        case TK::KW_CONTINUE: return parseContinue();
        case TK::KW_RETURN: return parseReturn();
        case TK::KW_PRINT:  return parsePrint();
        case TK::IDENT:
            // Look ahead: IDENT '=' is an assignment; otherwise expression-stmt
            if (peek(1).type == TK::ASSIGN) {
                std::string name = advance().value; // consume IDENT
                advance();                          // consume '='
                auto val = parseExpr();
                expect(TK::SEMICOLON, "Expected ';' after assignment");
                return makeAssign(name, std::move(val), ln);
            }
            [[fallthrough]];
        default: {
            auto e = parseExpr();
            expect(TK::SEMICOLON, "Expected ';' after expression");
            return makeExprStmt(std::move(e), ln);
        }
    }
}

StmtPtr Parser::parseVarDecl() {
    int ln = peek().line;
    advance(); // consume 'var'
    std::string name = expect(TK::IDENT, "Expected variable name").value;
    expect(TK::ASSIGN, "Expected '=' in var declaration");
    auto init = parseExpr();
    expect(TK::SEMICOLON, "Expected ';' after var declaration");
    return makeVarDecl(name, std::move(init), ln);
}

StmtPtr Parser::parseIfStmt() {
    int ln = peek().line;
    advance(); // consume 'if'
    expect(TK::LPAREN, "Expected '(' after 'if'");
    auto cond = parseExpr();
    expect(TK::RPAREN, "Expected ')' after condition");
    auto thenB = parseBlock();
    std::vector<StmtPtr> elseB;
    if (match(TK::KW_ELSE))
        elseB = parseBlock();
    return makeIf(std::move(cond), std::move(thenB), std::move(elseB), ln);
}

StmtPtr Parser::parseWhileStmt() {
    int ln = peek().line;
    advance(); // consume 'while'
    expect(TK::LPAREN, "Expected '(' after 'while'");
    auto cond = parseExpr();
    expect(TK::RPAREN, "Expected ')' after condition");
    auto body = parseBlock();
    return makeWhile(std::move(cond), std::move(body), ln);
}

std::vector<StmtPtr> Parser::parseForStmt() {   // desugar 'for' into 'while'
    int ln = peek().line;
    advance(); // consume 'for'
    expect(TK::LPAREN, "Expected '(' after 'for'");

    // init clause (var decl or assignment, includes its ';')
    StmtPtr init;
    if (check(TK::KW_VAR)) {
        init = parseVarDecl(); // handles 'var x = expr;'
    } else if (check(TK::IDENT) && peek(1).type == TK::ASSIGN) {
        int iln = peek().line;
        std::string name = advance().value; // consume IDENT
        advance();                          // consume '='
        auto val = parseExpr();
        expect(TK::SEMICOLON, "Expected ';' after for-init");
        init = makeAssign(name, std::move(val), iln);
    } else {
        throw std::runtime_error(
            "Expected var declaration or assignment as for-init (line "
            + std::to_string(peek().line) + ")");
    }

    // condition clause
    auto cond = parseExpr();
    expect(TK::SEMICOLON, "Expected ';' after for-condition");

    // post clause: assignment only, no trailing ';' (ends at ')')
    int pln = peek().line;
    std::string postName = expect(TK::IDENT, "Expected assignment in for-post").value;
    expect(TK::ASSIGN, "Expected '=' in for-post assignment");
    auto postVal = parseExpr();
    StmtPtr post = makeAssign(postName, std::move(postVal), pln);

    expect(TK::RPAREN, "Expected ')' after for-post");

    // body
    auto body = parseBlock();

    // Append post to end of body.
    body.push_back(std::move(post));

    std::vector<StmtPtr> result;
    result.push_back(std::move(init));
    result.push_back(makeWhile(std::move(cond), std::move(body), ln));
    return result;
}

StmtPtr Parser::parseBreak() {
    int ln = peek().line;
    advance(); // consume 'break'
    expect(TK::SEMICOLON, "Expected ';' after break");
    return makeBreak(ln);
}

StmtPtr Parser::parseContinue() {
    int ln = peek().line;
    advance(); // consume 'continue'
    expect(TK::SEMICOLON, "Expected ';' after continue");
    return makeContinue(ln);
}

StmtPtr Parser::parseReturn() {
    int ln = peek().line;
    advance(); // consume 'return'
    auto val = parseExpr();
    expect(TK::SEMICOLON, "Expected ';' after return");
    return makeReturn(std::move(val), ln);
}

StmtPtr Parser::parsePrint() {
    int ln = peek().line;
    advance(); // consume 'print'
    expect(TK::LPAREN, "Expected '(' after 'print'");
    auto val = parseExpr();
    expect(TK::RPAREN, "Expected ')' after print argument");
    expect(TK::SEMICOLON, "Expected ';' after print");
    return makePrint(std::move(val), ln);
}

ExprPtr Parser::parseExpr()    { return parseOrExpr(); }

ExprPtr Parser::parseOrExpr() {
    auto lhs = parseAndExpr();
    while (check(TK::OR)) {
        int ln = peek().line; advance();
        lhs = makeBinOp(BinOp::Or, std::move(lhs), parseAndExpr(), ln);
    }
    return lhs;
}

ExprPtr Parser::parseAndExpr() {
    auto lhs = parseCmpExpr();
    while (check(TK::AND)) {
        int ln = peek().line; advance();
        lhs = makeBinOp(BinOp::And, std::move(lhs), parseCmpExpr(), ln);
    }
    return lhs;
}

ExprPtr Parser::parseCmpExpr() {
    auto lhs = parseAddExpr();
    BinOp op{};
    bool  found = true;
    int   ln = peek().line;
    switch (peek().type) {
        case TK::EQ:  op = BinOp::Eq;  break;
        case TK::NEQ: op = BinOp::Neq; break;
        case TK::LT:  op = BinOp::Lt;  break;
        case TK::GT:  op = BinOp::Gt;  break;
        case TK::LEQ: op = BinOp::Leq; break;
        case TK::GEQ: op = BinOp::Geq; break;
        default:      found = false;   break;
    }
    if (found) {
        advance();
        lhs = makeBinOp(op, std::move(lhs), parseAddExpr(), ln);
    }
    return lhs;
}

ExprPtr Parser::parseAddExpr() {
    auto lhs = parseMulExpr();
    while (check(TK::PLUS) || check(TK::MINUS)) {
        BinOp op = check(TK::PLUS) ? BinOp::Add : BinOp::Sub;
        int ln = peek().line; advance();
        lhs = makeBinOp(op, std::move(lhs), parseMulExpr(), ln);
    }
    return lhs;
}

ExprPtr Parser::parseMulExpr() {
    auto lhs = parseUnary();
    while (check(TK::STAR) || check(TK::SLASH) || check(TK::PERCENT)) {
        BinOp op = check(TK::STAR)  ? BinOp::Mul : check(TK::SLASH) ? BinOp::Div : BinOp::Mod;
        int ln = peek().line; advance();
        lhs = makeBinOp(op, std::move(lhs), parseUnary(), ln);
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    if (check(TK::MINUS)) {
        int ln = peek().line; advance();
        return makeUnary(UnaryOp::Neg, parseUnary(), ln);
    }
    if (check(TK::BANG)) {
        int ln = peek().line; advance();
        return makeUnary(UnaryOp::Not, parseUnary(), ln);
    }
    return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
    int ln = peek().line;

    // Integer literal
    if (check(TK::INT_LIT)) {
        int v = std::stoi(advance().value);
        return makeIntLit(v, ln);
    }

    // Identifier or function call
    if (check(TK::IDENT)) {
        std::string name = advance().value;
        if (match(TK::LPAREN)) {
            // Function call
            std::vector<ExprPtr> args;
            if (!check(TK::RPAREN)) {
                args.push_back(parseExpr());
                while (match(TK::COMMA))
                    args.push_back(parseExpr());
            }
            expect(TK::RPAREN, "Expected ')' after arguments");
            return makeCall(name, std::move(args), ln);
        }
        return makeVar(name, ln);
    }

    // Grouped expression
    if (match(TK::LPAREN)) {
        auto e = parseExpr();
        expect(TK::RPAREN, "Expected ')' after grouped expression");
        return e;
    }

    throw std::runtime_error("Unexpected token '" + peek().value + "' at line " + std::to_string(ln));
}
