# Elio

A compiled, statically typed programming language with a focus on determinism and safety.
Elio targets LLVM IR and compiles to native x86-64 via `clang`.

## Quick start

```bash
# 1. Build
mkdir build && cd build
cmake ..
cmake --build .

# 2. Compile an Elio program
./elio ../examples/bst.el -o bst.ll

# 3. Link and run
clang -x ir bst.ll -o bst
./bst
```

## Build requirements

| Tool | Minimum version |
|------|----------------|
| CMake | 3.28 |
| clang++ | 18 |
| LLVM | 18 |
| clang-scan-deps | 18 |

## Build options

```bash
# Standard build
cmake ..
cmake --build .

# With AddressSanitizer and UBSan
cmake -DSANITIZE=ON ..
cmake --build .

# Available make targets
cmake --build . --target build   # rebuild
cmake --build . --target run     # compile + run examples/bst.el
cmake --build . --target debug   # debug build
cmake --build . --target clean   # clean object files
```

## Compiler flags

```
elio <source.el> [-o <output.ll>] [--dump-tokens] [--dump-ast] [--opt]

  -o <file>        LLVM IR output path (default: <source>.ll)
  --dump-tokens    Print the token stream and exit
  --dump-ast       Print the AST and exit
  --opt            Run LLVM optimisation passes
```

## Language overview

Elio is statically typed with explicit type annotations and a fully type-inferred `:=` form.
It has value semantics, lexical scoping, and guaranteed deterministic overload resolution.

```elio
fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

fn main() -> int32 {
    let x := 10;
    let y: int32 = 20;
    print(add(x, y));   // 30
    return 0;
}
```

See `specs/` for the full language specification and `examples/` for sample programs.

## Project structure

```
elio/
  src/            compiler source (C++23 modules)
    include/      tokens, expr, lexer, parser
    errors/       diagnostics
    syntax/       resolver, symbol table, typechecker
    codegen/      LLVM IR generation
  specs/          language specification
    grammar.md    EBNF grammar
    semantics.md  semantic rules
    types.md      type system
    codegen.md    code generation notes
  examples/       sample Elio programs
  tests/          test programs (including expected-error cases)
  main.cpp        compiler entry point
  CMakeLists.txt
  report.md
  README.md
```

## Running tests

```bash
# Compile and run a test
./elio tests/01_arithmetic.el -o /tmp/t.ll && clang -x ir /tmp/t.ll -o /tmp/t && /tmp/t

# Expected-error tests should fail to compile
./elio tests/err_div_zero.el 2>&1 | grep "runtime error"
```

## License

Academic project. All rights reserved.
