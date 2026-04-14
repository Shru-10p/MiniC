#include "codegen.hpp"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/BasicBlock.h"

#include <stdexcept>
#include <system_error>

CodeGen::CodeGen(const std::string& moduleName)
    : module_(std::make_unique<llvm::Module>(moduleName, ctx_)), builder_(ctx_) {}

llvm::Type* CodeGen::i32() { return llvm::Type::getInt32Ty(ctx_); }
llvm::Type* CodeGen::i1()  { return llvm::Type::getInt1Ty(ctx_);  }

llvm::ConstantInt* CodeGen::constI32(int v) {
    return llvm::ConstantInt::get(ctx_, llvm::APInt(32, v, /*isSigned=*/true));
}

llvm::AllocaInst* CodeGen::createEntryAlloca(llvm::Function* fn, const std::string& name) {
    // Point a temporary builder at the very first instruction of the entry block so
    // all allocas end up together — required by mem2reg.
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp.CreateAlloca(i32(), nullptr, name);
}

void CodeGen::ensurePrintf() {
    if (printfFn_) return;

    // i32 printf(i8* fmt, ...)
    llvm::FunctionType* ft = llvm::FunctionType::get(
        i32(),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0)},
        /*isVarArg=*/true);

    printfFn_ = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "printf", module_.get());
}

void CodeGen::dump() const {
    module_->print(llvm::outs(), nullptr);
}

void CodeGen::writeIR(const std::string& path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec);
    if (ec) throw std::runtime_error("Cannot open " + path + ": " + ec.message());
    module_->print(out, nullptr);
}

void CodeGen::writeBitcode(const std::string& path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("Cannot open " + path + ": " + ec.message());
    llvm::WriteBitcodeToFile(*module_, out);
}

void CodeGen::compile(const Program& prog) {
    genFuncDecls(prog);          // pass 1: declare all signatures
    for (auto& fn : prog.funcs)  // pass 2: emit bodies
        genFuncDef(fn);
    llvm::verifyModule(*module_, &llvm::errs());
}

void CodeGen::genFuncDecls(const Program& prog) {
    for (auto& fn : prog.funcs) {
        // All params and return value are i32
        std::vector<llvm::Type*> params(fn.params.size(), i32());
        llvm::FunctionType* ft = llvm::FunctionType::get(i32(), params, false);
        llvm::Function* f = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, fn.name, module_.get());

        // Name the parameters so IR is readable
        size_t idx = 0;
        for (auto& arg : f->args())
            arg.setName(fn.params[idx++]);

        funcTable_[fn.name] = f;
    }
}

void CodeGen::genFuncDef(const FuncDef& fn) {
    llvm::Function* f = funcTable_.at(fn.name);
    varTable_.clear(); // new scope per function
    loopStack_.clear();

    // Entry basic block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx_, "entry", f);
    builder_.SetInsertPoint(entry);

    // Allocate stack slots for parameters and store incoming values
    size_t idx = 0;
    for (auto& arg : f->args()) {
        const std::string& pname = fn.params[idx++];
        llvm::AllocaInst* slot = createEntryAlloca(f, pname);
        builder_.CreateStore(&arg, slot);
        varTable_[pname] = slot;
    }

    // Emit statements
    genStmts(fn.body);

    // If the last block has no terminator, emit a default "return 0"
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateRet(constI32(0));
}

void CodeGen::genStmts(const std::vector<StmtPtr>& stmts) {
    for (auto& s : stmts) {
        // Stop emitting if the current block is already terminated
        // (e.g. after a return inside an if-branch)
        if (builder_.GetInsertBlock()->getTerminator()) break;
        genStmt(*s);
    }
}

void CodeGen::genStmt(const Stmt& s) {
    llvm::Function* fn = builder_.GetInsertBlock()->getParent();

    switch (s.kind) {

    // var x = expr;
    case StmtKind::VarDecl: {
        if (varTable_.count(s.name))
            throw std::runtime_error("Variable '" + s.name + "' already declared");
        llvm::AllocaInst* slot = createEntryAlloca(fn, s.name);
        varTable_[s.name] = slot;
        llvm::Value* val = genExpr(*s.expr);
        builder_.CreateStore(val, slot);
        break;
    }

    // x = expr;
    case StmtKind::Assign: {
        auto it = varTable_.find(s.name);
        if (it == varTable_.end())
            throw std::runtime_error("Undefined variable '" + s.name + "'");
        builder_.CreateStore(genExpr(*s.expr), it->second);
        break;
    }

    // if (cond) { ... } else { ... }
    case StmtKind::If: {
        llvm::Value* condVal = genExpr(*s.cond);
        // Convert i32 → i1 (non-zero is true)
        condVal = builder_.CreateICmpNE(condVal, constI32(0), "ifcond");

        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(ctx_, "then",  fn);
        llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(ctx_, "else");
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(ctx_, "ifcont");

        builder_.CreateCondBr(condVal, thenBB, elseBB);

        // then
        builder_.SetInsertPoint(thenBB);
        genStmts(s.thenBody);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(mergeBB);

        // else
        fn->insert(fn->end(), elseBB);
        builder_.SetInsertPoint(elseBB);
        if (!s.elseBody.empty()) genStmts(s.elseBody);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(mergeBB);

        // merge
        fn->insert(fn->end(), mergeBB);
        builder_.SetInsertPoint(mergeBB);
        break;
    }

    // while (cond) { ... }
    case StmtKind::While: {
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(ctx_, "whcond",  fn);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(ctx_, "whbody");
        llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(ctx_, "whafter");

        builder_.CreateBr(condBB); // fall into condition check

        // condition
        builder_.SetInsertPoint(condBB);
        llvm::Value* condVal = genExpr(*s.cond);
        condVal = builder_.CreateICmpNE(condVal, constI32(0), "whcond");
        builder_.CreateCondBr(condVal, bodyBB, afterBB);

        // body
        fn->insert(fn->end(), bodyBB);
        builder_.SetInsertPoint(bodyBB);
        loopStack_.push_back({condBB, afterBB});
        genStmts(s.loopBody);
        loopStack_.pop_back();
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(condBB); // back-edge

        // after
        fn->insert(fn->end(), afterBB);
        builder_.SetInsertPoint(afterBB);
        break;
    }

    // break;
    case StmtKind::Break: {
        if (loopStack_.empty())
            throw std::runtime_error("'break' used outside loop at line " + std::to_string(s.line));
        builder_.CreateBr(loopStack_.back().breakBB);
        break;
    }

    // continue;
    case StmtKind::Continue: {
        if (loopStack_.empty())
            throw std::runtime_error("'continue' used outside loop at line " + std::to_string(s.line));
        builder_.CreateBr(loopStack_.back().continueBB);
        break;
    }

    // return expr;
    case StmtKind::Return: {
        builder_.CreateRet(genExpr(*s.expr));
        break;
    }

    // print(expr);
    case StmtKind::Print: {
        ensurePrintf();
        llvm::Value* val = genExpr(*s.expr);
        llvm::Value* fmt = builder_.CreateGlobalStringPtr("%d\n", "pfmt");
        builder_.CreateCall(printfFn_->getFunctionType(), printfFn_, {fmt, val});
        break;
    }

    // expr;
    case StmtKind::ExprStmt:
        genExpr(*s.expr); // generate for side-effects (e.g. function calls)
        break;
    }
}


llvm::Value* CodeGen::genExpr(const Expr& e) {
    switch (e.kind) {

    // literal
    case ExprKind::IntLit:
        return constI32(e.intVal);

    // variable
    case ExprKind::Var: {
        auto it = varTable_.find(e.name);
        if (it == varTable_.end())
            throw std::runtime_error("Undefined variable '" + e.name + "'");
        return builder_.CreateLoad(i32(), it->second, e.name);
    }

    // a OP b
    case ExprKind::BinOp: {
        // Short-circuit evaluation for logical operators
        if (e.binOp == BinOp::And || e.binOp == BinOp::Or) {
            llvm::Function* fn = builder_.GetInsertBlock()->getParent();
            llvm::AllocaInst* result = createEntryAlloca(fn, "logres");

            llvm::Value* lv = genExpr(*e.lhs);
            llvm::Value* lb = builder_.CreateICmpNE(lv, constI32(0));
            builder_.CreateStore(lv, result);

            llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(ctx_, "rhs", fn);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(ctx_, "logend");

            if (e.binOp == BinOp::And)
                builder_.CreateCondBr(lb, rhsBB, endBB);  // false → skip rhs
            else
                builder_.CreateCondBr(lb, endBB, rhsBB);  // true  → skip rhs

            // Evaluate RHS
            builder_.SetInsertPoint(rhsBB);
            llvm::Value* rv = genExpr(*e.rhs);
            // Normalise to 0/1
            llvm::Value* rb = builder_.CreateICmpNE(rv, constI32(0));
            llvm::Value* r1 = builder_.CreateZExt(rb, i32());
            builder_.CreateStore(r1, result);
            builder_.CreateBr(endBB);

            fn->insert(fn->end(), endBB);
            builder_.SetInsertPoint(endBB);
            // Normalise lhs result too
            llvm::Value* loaded = builder_.CreateLoad(i32(), result);
            llvm::Value* norm = builder_.CreateICmpNE(loaded, constI32(0));
            return builder_.CreateZExt(norm, i32(), "logval");
        }

        llvm::Value* lv = genExpr(*e.lhs);
        llvm::Value* rv = genExpr(*e.rhs);

        switch (e.binOp) {
            case BinOp::Add: return builder_.CreateAdd(lv, rv, "add");
            case BinOp::Sub: return builder_.CreateSub(lv, rv, "sub");
            case BinOp::Mul: return builder_.CreateMul(lv, rv, "mul");
            case BinOp::Div: return builder_.CreateSDiv(lv, rv, "div");
            case BinOp::Mod: return builder_.CreateSRem(lv, rv, "mod");
            // Comparisons → i1 then zero-extend to i32
            case BinOp::Eq: {
                auto cmp = builder_.CreateICmpEQ(lv, rv, "eq");
                return builder_.CreateZExt(cmp, i32());
            }
            case BinOp::Neq: {
                auto cmp = builder_.CreateICmpNE(lv, rv, "neq");
                return builder_.CreateZExt(cmp, i32());
            }
            case BinOp::Lt: {
                auto cmp = builder_.CreateICmpSLT(lv, rv, "lt");
                return builder_.CreateZExt(cmp, i32());
            }
            case BinOp::Gt: {
                auto cmp = builder_.CreateICmpSGT(lv, rv, "gt");
                return builder_.CreateZExt(cmp, i32());
            }
            case BinOp::Leq: {
                auto cmp = builder_.CreateICmpSLE(lv, rv, "leq");
                return builder_.CreateZExt(cmp, i32());
            }
            case BinOp::Geq: {
                auto cmp = builder_.CreateICmpSGE(lv, rv, "geq");
                return builder_.CreateZExt(cmp, i32());
            }
            default: break;
        }
        break;
    }

    // -x, !x
    case ExprKind::UnaryOp: {
        llvm::Value* v = genExpr(*e.operand);
        if (e.unaryOp == UnaryOp::Neg) return builder_.CreateNeg(v, "neg");
        // Logical NOT: (v == 0) --> 1, else 0
        auto cmp = builder_.CreateICmpEQ(v, constI32(0), "not");
        return builder_.CreateZExt(cmp, i32());
    }

    // f(args...)
    case ExprKind::Call: {
        auto it = funcTable_.find(e.name);
        if (it == funcTable_.end())
            throw std::runtime_error("Undefined function '" + e.name + "'");

        llvm::Function* callee = it->second;
        if (callee->arg_size() != e.args.size())
            throw std::runtime_error("Wrong argument count for '" + e.name + "'");

        std::vector<llvm::Value*> argVals;
        for (auto& a : e.args) argVals.push_back(genExpr(*a));
        return builder_.CreateCall(callee, argVals, "call");
    }
    }

    throw std::runtime_error("Internal codegen error: unhandled expression");
}
