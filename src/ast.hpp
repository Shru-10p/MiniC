#pragma once
#include <string>
#include <vector>
#include <memory>

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

enum class ExprKind { IntLit, Var, BinOp, UnaryOp, Call };

enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Gt, Leq, Geq,
    And, Or
};

enum class UnaryOp { Neg, Not };

struct Expr {
    ExprKind kind;
    int line = 0;

    // IntLit
    int intVal = 0;

    // Var / Call
    std::string name;

    // BinOp
    BinOp binOp{};
    ExprPtr lhs, rhs;

    // UnaryOp
    UnaryOp unaryOp{};
    ExprPtr operand;

    // Call args
    std::vector<ExprPtr> args;
};


inline ExprPtr makeIntLit(int v, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::IntLit;
    e->intVal = v;
    e->line = line;
    return e;
}

inline ExprPtr makeVar(std::string name, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Var;
    e->name = std::move(name);
    e->line = line;
    return e;
}

inline ExprPtr makeBinOp(BinOp op, ExprPtr lhs, ExprPtr rhs, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::BinOp;
    e->binOp = op;
    e->lhs = std::move(lhs);
    e->rhs = std::move(rhs);
    e->line = line;
    return e;
}

inline ExprPtr makeUnary(UnaryOp op, ExprPtr operand, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::UnaryOp;
    e->unaryOp = op;
    e->operand = std::move(operand);
    e->line = line;
    return e;
}

inline ExprPtr makeCall(std::string callee, std::vector<ExprPtr> args, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Call;
    e->name = std::move(callee);
    e->args = std::move(args);
    e->line = line;
    return e;
}

enum class StmtKind { VarDecl, Assign, If, While, Return, Print, ExprStmt };

struct Stmt {
    StmtKind kind;
    int      line = 0;

    // VarDecl / Assign: name + init/value
    std::string name;
    ExprPtr expr;   // init for VarDecl, value for Assign/Return/Print/ExprStmt

    // If / While: cond + body + optional else
    ExprPtr cond;
    std::vector<StmtPtr> thenBody;
    std::vector<StmtPtr> elseBody;  // empty = no else
    std::vector<StmtPtr> loopBody;  // while body
};

// Convenience constructors

inline StmtPtr makeVarDecl(std::string name, ExprPtr init, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::VarDecl;
    s->name = std::move(name);
    s->expr = std::move(init);
    s->line = line;
    return s;
}

inline StmtPtr makeAssign(std::string name, ExprPtr val, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Assign;
    s->name = std::move(name);
    s->expr = std::move(val);
    s->line = line;
    return s;
}

inline StmtPtr makeIf(ExprPtr cond, std::vector<StmtPtr> thenB, std::vector<StmtPtr> elseB, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::If;
    s->cond = std::move(cond);
    s->thenBody = std::move(thenB);
    s->elseBody = std::move(elseB);
    s->line = line;
    return s;
}

inline StmtPtr makeWhile(ExprPtr cond, std::vector<StmtPtr> body, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::While;
    s->cond = std::move(cond);
    s->loopBody = std::move(body);
    s->line = line;
    return s;
}

inline StmtPtr makeReturn(ExprPtr val, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Return;
    s->expr = std::move(val);
    s->line = line;
    return s;
}

inline StmtPtr makePrint(ExprPtr val, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Print;
    s->expr = std::move(val);
    s->line = line;
    return s;
}

inline StmtPtr makeExprStmt(ExprPtr e, int line = 0) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ExprStmt;
    s->expr = std::move(e);
    s->line = line;
    return s;
}

struct FuncDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    int line = 0;
};

struct Program {
    std::vector<FuncDef> funcs;
};
