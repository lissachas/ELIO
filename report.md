# Elio Compiler -- Project Report

## Architecture

The Elio compiler is a classical pipeline with five phases, each implemented as a
separate C++23 module. Phases communicate through a shared AST and a single
`Diagnostics` object; no phase modifies the output of a prior phase after it has
completed.

```
Source text
   |
   v
Lexer          (lexer.cppm)       -- character stream -> token stream
   |
   v
Parser         (parser.cppm)      -- token stream -> AST
   |
   v
Resolver       (resolve.cppm)     -- name resolution, scope analysis, mutability checks
   |
   v
TypeChecker    (typecheck.cppm)   -- type inference, type checking, exhaustiveness
   |
   v
Codegen        (codegen.cppm)     -- annotated AST -> LLVM IR
   |
   v
LLVM IR (.ll file)
```

### Lexer

Hand-written character-by-character scanner. Recognises all token classes including
decimal, hexadecimal (`0x`), and binary (`0b`) integer literals; float literals;
character literals with escape sequences; multi-line string literals; single-line
comments (`//`); block comments (`/* ... */` with nesting); and all operator tokens
including compound assignments (`+=`, `-=`, etc.) and shift operators (`<<`, `>>`).

Each token carries a source file, line number, and column number for precise diagnostics.

### Parser

Recursive-descent parser producing a heterogeneous AST. Arena allocation is used for
all AST nodes to avoid heap fragmentation and to give predictable lifetimes: the arena
owns all nodes and frees them in a single operation at the end of compilation.

All string-view members on AST nodes reference slices of the persistent `source` string
held by the `Lexer`; no string copies are made during parsing.

The parser performs error recovery at the statement level: on a parse error it
synchronises to the next statement keyword and continues, accumulating multiple errors
per compilation run.

### Resolver

Two-pass: a pre-pass registers all top-level declarations (functions, structs, type
aliases, constants, namespaces, enum constructors), then a main pass resolves every
identifier to its declaration. Scopes form a linked list of `Scope` nodes;
`lookup_var`/`lookup_fn`/`lookup_type` walk the chain.

The resolver enforces: use-before-declare within function bodies, duplicate names,
break/continue outside loops, and immutability of `let` bindings.

Function overloading is supported: the symbol table maps a function name to a
`vector<FnSymbol>`, so multiple signatures coexist. The resolver fills
`CallExpr::candidates`; the typechecker selects the winning overload.

### TypeChecker

The typechecker walks the annotated AST and either infers or checks every expression
and statement. Results are cached in a `node_type` map keyed by `Node*`, so downstream
phases can query any node's type without re-inferring.

Design decisions:

- **No implicit widening in the general case.** The only implicit casts allowed are at
  overloaded call sites, and only when exactly one overload is reachable by lossless
  widening (A.3.1). Any ambiguity is a hard compile error. This is Elio's determinism
  guarantee.
- **Scope save/restore on block entry.** `var_types` is a flat map; entering a block
  saves a copy and restores it on exit. This is O(n) in the number of visible variables,
  acceptable for a student project where scopes are small.
- **Pre-pass for top-level constants.** Constants at module scope are registered before
  function bodies are checked so any function may reference them.

### Code Generator

The codegen walks the annotated AST and emits LLVM IR using `llvm::IRBuilder<>`.

Key choices:
- **alloca + store + load pattern** for all mutable locals. The `mem2reg` pass (run
  during optimisation) promotes these to SSA registers.
- **Mangled LLVM symbol names** for overloaded functions: `name$type1$type2`, so
  distinct overloads have distinct symbols. `main` is never mangled.
- **Tagged-union layout for ADTs**: `{ i32 tag, [N x i8] payload }` where N is the size
  of the largest variant's payload struct. Match dispatch uses a chain of conditional
  branches rather than a switch instruction, supporting guards and multiple arms per tag.
- **Fat pointer `{ i8*, i64 }` for strings**: pointer to UTF-8 bytes plus byte length.
  All string operations (concatenation, equality, `print`) work on this struct.
  Concatenation calls `malloc` and `memcpy` from the C standard library.
- **Runtime error guards**: division by zero and array index out-of-bounds are checked
  at runtime with a conditional branch; on failure, the message
  `runtime error: <msg> at line <N>` is printed to stdout and `exit(1)` is called.

## Design decisions and rationale

### Determinism as a core principle

Elio's second design rule (after safety) is determinism: a program's behaviour must
not depend on unspecified resolution order. This rules out:
- Implicit numeric widening in arbitrary expression contexts
- Preference-based overload resolution (closest match wins)
- Undefined behaviour on integer overflow (Elio uses trap semantics via
  `wrap_add`/`wrap_sub`/`wrap_mul` builtins for intentional wraparound)

### Why LLVM IR

LLVM IR provides a portable, optimisable representation and handles all ABI details
(calling conventions, struct layout, x86-64 register allocation) automatically.
The `mem2reg` pass gives SSA form for free. This lets the compiler stay simple
(no register allocator, no instruction selection) while producing correct native code.

### Why not a visitor pattern

The AST uses a `NodeType` enum tag and switch dispatch. C++ virtual dispatch was
rejected because: it requires a class hierarchy that mirrors the AST structure
(adding nodes requires modifying multiple files), and the visitor pattern requires
forward declarations across module boundaries that conflict with C++23 module
import rules. Switch dispatch is explicit, localised, and trivially extensible.

### Arena allocation

All AST nodes are allocated in a single arena. This eliminates per-node `delete`,
removes the risk of use-after-free through raw `Node*` pointers in the AST, and
gives better cache behaviour than scattered heap allocations.

## Known limitations

- Lambdas: lambda syntax is parsed but closures (capture of outer variables) are not
  implemented. Lambdas currently cannot be used. This is a documented deviation.
- Namespace member structs: struct declarations inside a `namespace {}` block are
  registered but their LLVM struct type is not currently prefixed; accessing them
  from outside the namespace by qualified name may fail. Namespace functions work fully.
- String comparison operators `<`, `<=`, `>`, `>=` are not implemented (lexicographic
  order). Only `==` and `!=` are supported on strings.
- Array equality is implemented for arrays of numeric and bool elements only; arrays
  of structs or strings cannot be compared with `==`.
- The `for x in arr` loop requires a statically-sized array; iterator protocols are
  not implemented.

## Build and run

```bash
mkdir build && cd build
cmake ..
cmake --build .
./elio ../examples/bst.el -o bst.ll
clang -x ir bst.ll -o bst && ./bst
```

## Implemented addons

| ID | Level | Description |
|----|-------|-------------|
| A.1.1 | 1 | Numeric types of multiple sizes (int8..int64, uint8..uint64, flo32, flo64) |
| A.1.2 | 1 | Unicode strings (UTF-8 fat pointer; byte-length semantics documented) |
| A.1.5 | 1 | Plain enums (zero-payload ADT variants; prerequisite for A.2.3) |
| A.1.9 | 1 | Extended operators: bitwise (AND, OR, XOR, NOT, SHL, SHR) and compound assignments |
| A.1.15 | 1 | Block comments with nesting support |
| A.2.3 | 2 | Algebraic data types (ADT) and pattern matching |
| A.2.8 | 2 | Function overloading (exact-match resolution) |
| A.3.1 | 3 | Overloading with implicit widening casts; ambiguity is a hard compile error |
| A.3.4 | 3 | Full pattern matching: exhaustiveness checking, guards, nested patterns |
