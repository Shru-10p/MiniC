# minicc

A minimal compiler for **MiniC**, a small C-like language. The compiler translates MiniC source into **LLVM IR**,
which you then feed to `clang` or `llc` to get a native binary.

```None
source.mc ────>  Lexer ────>  Parser ────>  AST ────>  CodeGen ────> source.ll ────LLVM IRBuilder───> clang / llc ────> native binary
```

## Table of Contents

- [minicc](#minicc)
  - [Table of Contents](#table-of-contents)
  - [Language (MiniC)](#language-minic)
    - [Examples](#examples)
    - [Grammar (EBNF)](#grammar-ebnf)
  - [Prerequisites](#prerequisites)
    - [Install LLVM](#install-llvm)
      - [Ubuntu / Debian](#ubuntu--debian)
  - [Build](#build)
  - [Usage](#usage)
    - [Compile and run an example](#compile-and-run-an-example)
    - [Debug helpers](#debug-helpers)
    - [Optimise with `opt`](#optimise-with-opt)
  - [Architecture](#architecture)
    - [Lexer (`lexer.hpp / lexer.cpp`)](#lexer-lexerhpp--lexercpp)
    - [Parser (`parser.hpp / parser.cpp`)](#parser-parserhpp--parsercpp)
    - [AST (`ast.hpp`)](#ast-asthpp)
    - [CodeGen (`codegen.hpp / codegen.cpp`)](#codegen-codegenhpp--codegencpp)

## Language (MiniC)

| Feature          | Syntax                                               |
|------------------|------------------------------------------------------|
| Integer variable | `var x = 42;`                                        |
| Assignment       | `x = x + 1;`                                         |
| Arithmetic       | `+ - * / %`  (standard precedence)                   |
| Comparison       | `== != < > <= >=`                                    |
| Logical          | `&& \|\| !`  (short-circuit)                         |
| Conditional      | `if (cond) { … } else { … }`                         |
| While loop       | `while (cond) { … }`                                 |
| For loop         | `for (init; cond; post) { … }`  (desugars to while)  |
| Loop control     | `break;` / `continue;`                               |
| Functions        | `func name(a, b) { … }`                              |
| Return           | `return expr;`                                       |
| Print            | `print(expr);`  -> calls `printf("%d\n")`            |
| Comments         | `// line`  or  `/* block */`                         |

All values are **32-bit signed integers** (`i32`).

### Examples

```c
// Fibonacci with a while loop
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

```c
// Sum odd numbers in 1..10 using continue --> prints 25
func main() {
    var sum = 0;
    for (var i = 1; i <= 10; i = i + 1) {
        if (i % 2 == 0) {
            i = i + 1;
            continue;
        }
        sum = sum + i;
    }
    print(sum);
}
```

### Grammar (EBNF)

EBNF conventions:

| Notation | Meaning |
| -------- | ------- |
| `=` | rule definition |
| `;` | end of rule |
| `"..."` | literal terminal (keyword or punctuation) |
| `NAME`, `INT` | lexical terminals (identifier / integer literal) |
| `A , B` | A followed by B (sequencing) |
| `A \| B` | A or B (choice) |
| `[ A ]` | A is optional (zero or one) |
| `{ A }` | zero or more repetitions of A |
| `( A )` | grouping |

**Grammar:**

```ebnf
/* -- Top level -- */

program     = { funcdef }

funcdef     = "func" , NAME , "(" , [ params ] , ")" , block
params      = NAME , { "," , NAME }

block       = "{" , { stmt } , "}"

/* -- Statements -- */

stmt        = var-decl
            | assign
            | if-stmt
            | while-stmt
            | for-stmt
            | break-stmt
            | continue-stmt
            | return-stmt
            | print-stmt
            | expr-stmt

var-decl    = "var" , NAME , "=" , expr , ";"

assign      = NAME , "=" , expr , ";"

if-stmt     = "if" , "(" , expr , ")" , block ,
              [ "else" , block ]

while-stmt  = "while" , "(" , expr , ")" , block

for-stmt    = "for" , "(" , for-init , expr , ";" ,
              NAME , "=" , expr , ")" , block

for-init    = var-decl | assign    /* each includes its trailing ";" */

break-stmt  = "break" , ";"

continue-stmt = "continue" , ";"

return-stmt = "return" , expr , ";"

print-stmt  = "print" , "(" , expr , ")" , ";"

expr-stmt   = expr , ";"

/* -- Expressions (low to high precedence) -- */

expr        = or-expr

or-expr     = and-expr , { "||" , and-expr }

and-expr    = cmp-expr , { "&&" , cmp-expr }

cmp-expr    = add-expr ,
              [ ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) , add-expr ]

add-expr    = mul-expr , { ( "+" | "-" ) , mul-expr }

mul-expr    = unary , { ( "*" | "/" | "%" ) , unary }

unary       = "-" , unary
            | "!" , unary
            | primary

primary     = INT
            | NAME
            | NAME , "(" , [ args ] , ")"
            | "(" , expr , ")"

args        = expr , { "," , expr }
```

> **`for` desugaring** — `for (init; cond; post) { body }` is rewritten by the
> parser into `init; while (cond) { body; post; }` before the AST is built.
> Neither the AST nor the code generator has any knowledge of `for`.

> Current limitation: `continue` inside a `for` body jumps to the desugared
> `while` condition and skips `post`, so examples currently perform explicit
> increment before `continue`.

## Prerequisites

| Tool          | Version  | Install                                               |
|---------------|----------|-------------------------------------------------------|
| LLVM          | 16 – 18  | see below                                             |
| Clang or GCC  | C++17    | bundled with LLVM, or system package                  |
| CMake         | >=3.16   | `brew install cmake` / `apt install cmake`            |

### Install LLVM

#### Ubuntu / Debian

```bash
sudo apt install llvm-18-dev  # or llvm-15-dev, llvm-16-dev, llvm-17-dev
# CMake usually finds it automatically via llvm-config-18
```

## Build

```bash
git clone https://github.com/Shru-10p/MiniC.git
cd MiniC
mkdir build && cd build

cmake ..
cmake --build . -j$(nproc)
```

The `minicc` executable will be in `build/`.

## Usage

```bash
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
│   ├── funcs.mc          multi-param fns, recursion
│   └── forloop.mc        for loop + continue example (odd sum)
└── src/
    ├── lexer.hpp / .cpp   Tokeniser
    ├── ast.hpp            AST node structs (header-only)
    ├── parser.hpp / .cpp  Recursive-descent parser
    ├── codegen.hpp / .cpp LLVM IR emitter
    └── main.cpp
```

### Lexer (`lexer.hpp / lexer.cpp`)

Single-pass character scanner.  Handles:

- Integer literals, identifiers, keywords
- All operators including two-character tokens (`==`, `!=`, `<=`, `&&`, ...)
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
| `break`       | Branch to nearest loop `whafter` block                        |
| `continue`    | Branch to nearest loop condition block                        |
| `for`         | Desugared to `while` in the parser; no dedicated IR pattern   |
| `print`       | External `printf` declaration; `CreateGlobalStringPtr("%d\n")`|
| Functions     | Two-pass: forward-declare all sigs, then emit bodies          |
