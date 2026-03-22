#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <cstring>

static std::string binOpStr(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";  case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";  case BinOp::Div: return "/";
        case BinOp::Mod: return "%";  case BinOp::Eq:  return "==";
        case BinOp::Neq: return "!="; case BinOp::Lt:  return "<";
        case BinOp::Gt:  return ">";  case BinOp::Leq: return "<=";
        case BinOp::Geq: return ">="; case BinOp::And: return "&&";
        case BinOp::Or:  return "||"; default:         return "?";
    }
}

static void printExpr(const Expr& e, int indent = 0) {
    std::string pad(indent * 2, ' ');
    switch (e.kind) {
        case ExprKind::IntLit:
            std::cout << pad << "IntLit(" << e.intVal << ")\n"; break;
        case ExprKind::Var:
            std::cout << pad << "Var(" << e.name << ")\n"; break;
        case ExprKind::BinOp:
            std::cout << pad << "BinOp(" << binOpStr(e.binOp) << ")\n";
            printExpr(*e.lhs, indent + 1);
            printExpr(*e.rhs, indent + 1);
            break;
        case ExprKind::UnaryOp:
            std::cout << pad << "UnaryOp(" << (e.unaryOp == UnaryOp::Neg ? "-" : "!") << ")\n";
            printExpr(*e.operand, indent + 1);
            break;
        case ExprKind::Call:
            std::cout << pad << "Call(" << e.name << ")\n";
            for (auto& a : e.args) printExpr(*a, indent + 1);
            break;
    }
}

static void printStmt(const Stmt& s, int indent = 0);
static void printStmts(const std::vector<StmtPtr>& stmts, int indent) {
    for (auto& s : stmts) printStmt(*s, indent);
}

static void printStmt(const Stmt& s, int indent) {
    std::string pad(indent * 2, ' ');
    switch (s.kind) {
        case StmtKind::VarDecl:
            std::cout << pad << "VarDecl(" << s.name << ")\n";
            printExpr(*s.expr, indent + 1); break;
        case StmtKind::Assign:
            std::cout << pad << "Assign(" << s.name << ")\n";
            printExpr(*s.expr, indent + 1); break;
        case StmtKind::If:
            std::cout << pad << "If\n";
            std::cout << pad << "  Cond:\n"; printExpr(*s.cond, indent + 2);
            std::cout << pad << "  Then:\n"; printStmts(s.thenBody, indent + 2);
            if (!s.elseBody.empty()) {
                std::cout << pad << "  Else:\n"; printStmts(s.elseBody, indent + 2);
            }
            break;
        case StmtKind::While:
            std::cout << pad << "While\n";
            std::cout << pad << "  Cond:\n"; printExpr(*s.cond, indent + 2);
            std::cout << pad << "  Body:\n"; printStmts(s.loopBody, indent + 2);
            break;
        case StmtKind::Return:
            std::cout << pad << "Return\n"; printExpr(*s.expr, indent + 1); break;
        case StmtKind::Print:
            std::cout << pad << "Print\n"; printExpr(*s.expr, indent + 1); break;
        case StmtKind::ExprStmt:
            std::cout << pad << "ExprStmt\n"; printExpr(*s.expr, indent + 1); break;
    }
}

static void usage(const char* argv0) {
    std::cerr <<
        "Usage: " << argv0 << " [options] <source.mc>\n"
        "\n"
        "Options:\n"
        "  --dump-tokens     Print token stream and exit\n"
        "  --dump-ast        Print AST and exit\n"
        "  --emit-ir         Write LLVM IR (.ll) to <source>.ll  [default]\n"
        "  --emit-bc         Write LLVM bitcode (.bc) to <source>.bc\n"
        "  -o <file>         Override output file name\n"
        "\n"
        "Compile and run example:\n"
        "  ./minicc examples/arith.mc\n"
        "  clang -o arith arith.ll && ./arith\n";
}

int main(int argc, char** argv) {
    // Parse CLI
    bool dumpTokens = false;
    bool dumpAst    = false;
    bool emitBC     = false;
    std::string outFile;
    std::string srcFile;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump-tokens") == 0) dumpTokens = true;
        else if (strcmp(argv[i], "--dump-ast") == 0) dumpAst    = true;
        else if (strcmp(argv[i], "--emit-ir") == 0) emitBC     = false;
        else if (strcmp(argv[i], "--emit-bc") == 0) emitBC     = true;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outFile = argv[++i];
        else if (argv[i][0] != '-')
            srcFile = argv[i];
        else { usage(argv[0]); return 1; }
    }
    if (srcFile.empty()) { usage(argv[0]); return 1; }

    // Read source
    std::ifstream ifs(srcFile);
    if (!ifs) {
        std::cerr << "Error: cannot open '" << srcFile << "'\n";
        return 1;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string source = ss.str();

    // Lex
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        if (dumpTokens) {
            for (auto& t : tokens)
                std::cout << "Line " << t.line << "  [" << (int)t.type << "]  " << t.value << "\n";
            return 0;
        }

        // Parse
        Parser parser(std::move(tokens));
        Program prog = parser.parse();

        if (dumpAst) {
            for (auto& fn : prog.funcs) {
                std::cout << "FuncDef(" << fn.name << ")  params: [";
                for (size_t i = 0; i < fn.params.size(); ++i)
                    std::cout << fn.params[i] << (i + 1 < fn.params.size() ? ", " : "");
                std::cout << "]\n";
                printStmts(fn.body, 1);
            }
            return 0;
        }

        // Codegen
        CodeGen cg(srcFile);
        cg.compile(prog);

        // Determine output filename
        if (outFile.empty()) {
            // Strip directory and extension from srcFile
            std::string base = srcFile;
            auto slash = base.rfind('/');
            if (slash != std::string::npos) base = base.substr(slash + 1);
            auto dot = base.rfind('.');
            if (dot != std::string::npos) base = base.substr(0, dot);
            outFile = base + (emitBC ? ".bc" : ".ll");
        }

        if (emitBC) cg.writeBitcode(outFile);
        else cg.writeIR(outFile);

        std::cout << "Wrote " << outFile << "\n";
        std::cout << "compile: clang -o " << outFile.substr(0, outFile.rfind('.')) << " " << outFile << "\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
