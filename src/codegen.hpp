#pragma once
#include "ast.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"

#include <string>
#include <unordered_map>
#include <memory>

/*
    CodeGen walks the AST and emits LLVM IR.

    All variables are i32 (32-bit integer).
    Variables are stack-allocated with alloca; mem2reg will
    promote them to SSA registers during optimisation.
*/

class CodeGen {
public:
    explicit CodeGen(const std::string& moduleName);

    // Entry point: compile an entire program
    void compile(const Program& prog);

    // Print the generated LLVM IR to stdout
    void dump() const;

    // Write bitcode to a file (pass to `llc` or `clang`)
    void writeBitcode(const std::string& path) const;

    // Write human-readable IR (.ll) to a file
    void writeIR(const std::string& path) const;

private:
    llvm::LLVMContext ctx_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;

    // Function table: name → LLVM Function*
    std::unordered_map<std::string, llvm::Function*> funcTable_;

    // Variable table (per-function scope): name → alloca slot
    std::unordered_map<std::string, llvm::AllocaInst*> varTable_;

    // printf function pointer (declared once, reused by print stmts)
    llvm::Function* printfFn_ = nullptr;

    llvm::Type* i32();
    llvm::Type* i1();
    llvm::ConstantInt* constI32(int v);

    // Create an alloca at the entry-block of `fn` (stable position so mem2reg can promote it)
    llvm::AllocaInst* createEntryAlloca(llvm::Function* fn, const std::string& name);

    // Declare printf once into the module
    void ensurePrintf();

    void genFuncDecls(const Program& prog);   // forward-declare all funcs
    void genFuncDef(const FuncDef& fn);
    void genStmts(const std::vector<StmtPtr>& stmts);
    void genStmt(const Stmt& s);
    llvm::Value* genExpr(const Expr& e);
};
