# Elio Code Generation

## Approach

Elio compiles to **LLVM IR** via the `llvm::IRBuilder<>` API (LLVM 18/19).
The IR is written to a `.ll` text file and is subsequently compiled to a native
executable by `clang -x ir`.

Rationale: LLVM IR provides a clean target-independent instruction set that handles
calling conventions, struct layout, and register allocation automatically.
The `mem2reg` optimisation pass handles SSA promotion of stack variables,
so the code generator can use a simple alloca-based approach without implementing
its own register allocator.

## Compilation units

The Elio compiler produces a single LLVM module per source file. The module contains:
- One `llvm::Function` per function declaration, named and typed according to the
  rules below.
- One `llvm::StructType` per struct and enum declaration.
- One `llvm::GlobalVariable` for each string literal (private, constant, null-terminated).

## Variable representation

All local variables (including function parameters) are represented as stack allocations:

```llvm
%x = alloca i32
store i32 %arg, ptr %x
%val = load i32, ptr %x
```

The `mem2reg` pass (run when `--opt` is passed) promotes these to SSA registers,
eliminating the alloca/load/store pattern wherever the variable's address is not taken.

## Type mapping

| Elio type | LLVM type |
|-----------|-----------|
| `bool`    | `i1` |
| `char`    | `i32` |
| `int8` / `uint8` | `i8` |
| `int16` / `uint16` | `i16` |
| `int32` / `uint32` | `i32` |
| `int64` / `uint64` | `i64` |
| `flo32`   | `float` |
| `flo64`   | `double` |
| `unit`    | `void` (return type) or absent (struct field) |
| `string` / `string_view` / `buf_string` | `%string = { ptr, i64 }` |
| `[T; N]`  | `[N x llvm(T)]` |
| `struct S { f1: T1, f2: T2 }` | `%S = { llvm(T1), llvm(T2) }` |
| `enum E { V1(T), V2 }` | `%E = { i32, [N x i8] }` |
| `optional[T]` | `{ i1, llvm(T) }` |
| `result[T, E]` | `{ i1, llvm(T), llvm(E) }` |
| `&T` / `&mut T` | `ptr` |

## Function naming

Functions are named by **mangling** their signature to produce a unique LLVM symbol:

```
<name>$<param1_tag>$<param2_tag>...
```

Type tags: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`,
`b` (bool), `c` (char), `s` (string), `v` (unit), `S<name>` (struct), `E<name>` (enum),
`A<N>_<elem_tag>` (array), `O_<inner>` (optional), `R_<ok>_<err>` (result).

The entry point `main` is never mangled; it is always emitted as `main`.

Namespaced functions are prefixed: `namespace$mangled_name`.

## Calling convention

The default C calling convention is used for all functions. Arguments are passed
by value. Large aggregates (structs, arrays) are passed via the System V x86-64
ABI rules as implemented by LLVM's target lowering; `byval` attributes are inserted
automatically by LLVM when needed.

## String representation

Strings are fat pointers `%string = { ptr, i64 }`. Field 0 is a pointer to the
first byte of a null-terminated UTF-8 sequence. Field 1 is the byte length
(excluding the null terminator).

String literals are stored as private constant globals:
```llvm
@str.0 = private constant [6 x i8] c"hello\00"
```
and presented as `%string { ptr @str.0, i64 5 }`.

Concatenation calls `malloc` to allocate `len1 + len2` bytes and `memcpy` to
copy both halves. The resulting `{ ptr, i64 }` owns the allocation. Elio does not
free these allocations; they are reclaimed by the OS on process exit.

## ADT (enum) representation

An enum value is a tagged union:
```llvm
%Shape = type { i32, [N x i8] }
```
where N is the byte size of the largest variant's payload struct, determined at
compile time using `llvm::DataLayout::getTypeAllocSize`.

Construction stores the tag at index 0 and bitcasts index 1 to the variant's
payload struct type before storing the fields.

Match dispatch is a linear chain of conditional branches: each arm tests whether
the loaded tag equals the expected variant index, falls through to the next arm
on mismatch, or falls to the `unreachable` terminator (proven safe by the
exhaustiveness check).

## Runtime error handling

Before each division and modulo operation the divisor is tested against zero:
```llvm
%iszero = icmp eq i32 %rhs, 0
br i1 %iszero, label %div.zero, label %div.ok
div.zero:
  call void @printf(ptr @err_msg)
  call void @exit(i32 1)
  unreachable
div.ok:
  %result = sdiv i32 %lhs, %rhs
```

Array index operations insert a bounds check before the GEP:
```llvm
%idx64 = sext i32 %idx to i64
%oob   = icmp uge i64 %idx64, 16
br i1 %oob, label %idx.bad, label %idx.ok
```

The `unreachable` terminator after `exit(1)` tells LLVM this path cannot return,
allowing dead-code elimination of subsequent instructions.

## Short-circuit boolean operators

`&&` and `||` are compiled using conditional branches and PHI nodes rather than
eager evaluation:

```llvm
; a && b
  %left  = <evaluate a>
  br i1 %left, label %rhs, label %merge
rhs:
  %right = <evaluate b>
  br label %merge
merge:
  %result = phi i1 [ false, %entry ], [ %right, %rhs ]
```

## Optimisation

When `--opt` is passed, the following LLVM passes are run before emitting IR:
- `mem2reg`: promote stack allocations to SSA registers
- `instcombine`: algebraic simplification
- `simplifycfg`: dead block elimination, branch folding

Without `--opt`, no passes are run and the IR is emitted as produced by IRBuilder.
This makes the unoptimised IR easier to read and debug.
