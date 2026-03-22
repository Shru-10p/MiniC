# minicc — A Tiny C-Like Compiler via LLVM

A minimal compiler for **MiniC**, a small C-like language, built as a
learning project.  The compiler translates MiniC source into **LLVM IR**,
which you then feed to `clang` or `llc` to get a native binary.

```None
source.mc -->  Lexer -->  Parser -->  AST -->  CodeGen --> source.ll
                                                        │
                                                   LLVM IRBuilder
                                                        │
                                           clang / llc --> native binary
```

## Language (MiniC)

| Feature          | Syntax                                     |
|------------------|--------------------------------------------|
| Integer variable | `var x = 42;`                              |
| Assignment       | `x = x + 1;`                               |
| Arithmetic       | `+ - * / %`  (standard precedence)         |
| Comparison       | `== != < > <= >=`                          |
| Logical          | `&& \|\| !`  (short-circuit)               |
| Conditional      | `if (cond) { … } else { … }`               |
| Loop             | `while (cond) { … }`                       |
| Functions        | `func name(a, b) { … }`                    |
| Return           | `return expr;`                             |
| Print            | `print(expr);`  -> calls `printf("%d\n")`  |
| Comments         | `// line`  or  `/* block */`               |

All values are **32-bit signed integers** (`i32`).

### Example

```c
func fib(n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}

func main() {
    var i = 0;
    while (i < 10) {
        print(fib(i));
        i = i + 1;
    }
    return 0;
}
```

## Prerequisites

| Tool          | Version  | Install                                               |
|---------------|----------|-------------------------------------------------------|
| LLVM          | 15 – 18  | see below                                             |
| Clang or GCC  | C++17    | bundled with LLVM, or system package                  |
| CMake         | >=3.16   | `brew install cmake` / `apt install cmake`            |

### Install LLVM

#### macOS (Homebrew)

```bash
brew install llvm
# Tell CMake where to find it:
export LLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm
```

#### Ubuntu / Debian

```bash
sudo apt install llvm-18-dev  # or llvm-15-dev, llvm-16-dev, llvm-17-dev
# CMake usually finds it automatically via llvm-config-18
```

#### Fedora / RHEL

```bash
sudo dnf install llvm-devel
```

## Build

```bash
git clone <this-repo>
cd minicc
mkdir build && cd build

cmake ..
cmake --build . -j$(nproc)
```

The `minicc` executable will be in `build/`.

## Usage

```None
minicc [options] <source.mc>

  --dump-tokens     Print token stream and exit
  --dump-ast        Print AST and exit
  --emit-ir         Write LLVM IR (.ll)  [default]
  --emit-bc         Write LLVM bitcode (.bc)
  -o <file>         Override output filename
```

### Compile and run an example

```bash
# 1: MiniC --> LLVM IR
./build/minicc examples/arith.mc
# Produces: arith.ll

# 2: LLVM IR --> native binary  (pick one)
clang -o arith arith.ll          # via Clang
llc arith.ll -o arith.s && gcc -o arith arith.s   # via llc + linker

# 3: run
./arith
```

### Debug helpers

```bash
# Inspect the token stream
./build/minicc --dump-tokens examples/cond.mc

# Inspect the AST
./build/minicc --dump-ast examples/funcs.mc

# Inspect the generated IR
./build/minicc examples/funcs.mc && cat funcs.ll

# Run the IR directly with the LLVM interpreter
lli arith.ll
```

### Optimise with `opt`

```bash
# Run mem2reg + basic optimisation passes
opt -passes="mem2reg,instcombine,simplifycfg" arith.ll -o arith_opt.ll -S
cat arith_opt.ll
```

## Architecture

```None
minicc/
├── CMakeLists.txt
├── README.md
├── examples/
│   ├── arith.mc          arithmetic & precedence
│   ├── cond.mc           if/else, while, logic ops
│   └── funcs.mc          multi-param fns, recursion
└── src/
    ├── lexer.hpp / .cpp   Tokeniser
    ├── ast.hpp            AST node structs (header-only)
    ├── parser.hpp / .cpp  Recursive-descent parser
    ├── codegen.hpp / .cpp LLVM IR emitter
    └── main.cpp           CLI driver + AST printer
```

### Lexer (`lexer.hpp / lexer.cpp`)

Single-pass character scanner.  Handles:

- Integer literals, identifiers, keywords
- All operators including two-character tokens (`==`, `!=`, `<=`, `&&`, …)
- Line (`//`) and block (`/* */`) comments

### Parser (`parser.hpp / parser.cpp`)

Recursive-descent parser with layered precedence functions:

```None
parseExpr
└──> parseOrExpr
        └──> parseAndExpr
                └──> parseCmpExpr
                        └──> parseAddExpr
                                └──> parseMulExpr
                                        └──> parseUnary
                                                └──> parsePrimary
```

Produces a tree of `Expr` and `Stmt` nodes (defined in `ast.hpp`).

### AST (`ast.hpp`)

Plain structs with a `kind` discriminant and `std::unique_ptr` children.
No virtual dispatch — keeps the code easy to read and extend.

### CodeGen (`codegen.hpp / codegen.cpp`)

Walks the AST and calls `llvm::IRBuilder<>` methods:

| Construct     | LLVM IR strategy                                              |
|---------------|---------------------------------------------------------------|
| Variables     | `alloca i32` in entry block; `store` / `load` per use         |
| Arithmetic    | `add`, `sub`, `mul`, `sdiv`, `srem`                           |
| Comparisons   | `icmp s{lt,gt,le,ge,eq,ne}` → `zext i1 → i32`                 |
| `&&` / `\|\|` | Short-circuit: conditional branch around RHS evaluation       |
| `if`/`else`   | `then`, `else`, `ifcont` basic blocks + `br` / `condbr`       |
| `while`       | `whcond`, `whbody`, `whafter` blocks; back-edge to `whcond`   |
| `print`       | External `printf` declaration; `CreateGlobalStringPtr("%d\n")`|
| Functions     | Two-pass: forward-declare all sigs, then emit bodies          |

## Extending the language

Possible next steps:

- **Strings** — add a `str` type; lower to `i8*`; extend `print`
- **Arrays** — `var arr[10];` → `alloca [10 x i32]`; index with GEP
- **Boolean type** — separate `bool` from `int`; dedicated `i1` storage
- **For loop** — `for (init; cond; step)` — desugar to `while` in parser
- **Optimisation pass** — hook `llvm::PassManager`; run `mem2reg` + DCE
- **Type checker** — walk AST before codegen; catch type errors early
- **Multiple types** — `int` / `float` — add `FAdd`, `FMul`, LLVM `float`
