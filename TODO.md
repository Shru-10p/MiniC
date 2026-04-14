# TODO

- [~] **`break` / `continue`** - implemented for `while`; `continue` in desugared `for` currently skips post-clause
- [ ] **globals** - `var x = 42;` at top level -> `@x = global i32 42`
- [ ] **strings** - add a `str` type; lower to `i8*`; extend `print`
- [ ] **arrays** - `var arr[10];` -> `alloca [10 x i32]`; index with GEP
- [ ] **boolean type** - separate `bool` from `int`; dedicated `i1` storage
- [ ] **optimisation pass** - hook `llvm::PassManager`; run `mem2reg` + DCE
- [ ] **type checker** - walk AST before codegen; catch type errors early
- [ ] **multiple types** - `int` / `float`; add `FAdd`, `FMul`, LLVM `float`